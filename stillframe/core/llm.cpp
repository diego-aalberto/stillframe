#include "llm.hpp"
#include "http.hpp"
#include <algorithm>
#include <chrono>

namespace sf {

// ---------------------------------------------------------------------------- partial JSON

static void put_utf8(std::string& s, uint32_t cp) {
    if (cp < 0x80) s += static_cast<char>(cp);
    else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
}

std::string partial_string_field(std::string_view t, std::string_view key, bool* closed) {
    if (closed) *closed = false;
    // Walk the top-level object so that a "reply" inside a nested value is never mistaken for ours.
    size_t i = 0;
    while (i < t.size() && t[i] != '{') ++i;
    if (i == t.size()) return "";
    ++i;
    int depth = 1;
    auto skip_ws = [&] { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t' || t[i] == ',')) ++i; };
    while (i < t.size()) {
        skip_ws();
        if (i >= t.size()) return "";
        if (t[i] == '}') { if (--depth == 0) return ""; ++i; continue; }
        if (t[i] != '"') { ++i; continue; }
        // read a key
        size_t k0 = ++i;
        while (i < t.size() && t[i] != '"') { if (t[i] == '\\') ++i; ++i; }
        if (i >= t.size()) return "";
        std::string_view k = t.substr(k0, i - k0);
        ++i;
        skip_ws();
        if (i >= t.size() || t[i] != ':') continue;
        ++i;
        skip_ws();
        if (i >= t.size()) return "";
        if (k == key && t[i] == '"') {
            ++i;
            std::string out;
            while (i < t.size()) {
                char c = t[i];
                if (c == '"') { if (closed) *closed = true; return out; }
                if (c != '\\') { out += c; ++i; continue; }
                if (i + 1 >= t.size()) return out;             // lone backslash at the end: wait for more
                char e = t[i + 1];
                if (e == 'u') {
                    if (i + 6 > t.size()) return out;
                    uint32_t cp = static_cast<uint32_t>(std::strtoul(std::string(t.substr(i + 2, 4)).c_str(), nullptr, 16));
                    size_t adv = 6;
                    if (cp >= 0xD800 && cp < 0xDC00) {          // high surrogate: need the pair
                        if (i + 12 > t.size()) return out;
                        uint32_t lo = static_cast<uint32_t>(std::strtoul(std::string(t.substr(i + 8, 4)).c_str(), nullptr, 16));
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        adv = 12;
                    }
                    put_utf8(out, cp);
                    i += adv;
                    continue;
                }
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    default: out += e;
                }
                i += 2;
            }
            return out;
        }
        // skip a value that isn't ours
        if (t[i] == '"') {
            ++i;
            while (i < t.size() && t[i] != '"') { if (t[i] == '\\') ++i; ++i; }
            ++i;
        } else if (t[i] == '{' || t[i] == '[') {
            int d = 0; bool in_str = false;
            for (; i < t.size(); ++i) {
                char c = t[i];
                if (in_str) { if (c == '\\') ++i; else if (c == '"') in_str = false; continue; }
                if (c == '"') in_str = true;
                else if (c == '{' || c == '[') ++d;
                else if (c == '}' || c == ']') { if (--d == 0) { ++i; break; } }
            }
        } else {
            while (i < t.size() && t[i] != ',' && t[i] != '}') ++i;
        }
    }
    return "";
}

bool extract_json_object(std::string_view text, Json& out) {
    size_t start = 0;
    while (true) {
        start = text.find('{', start);
        if (start == std::string_view::npos) return false;
        int depth = 0; bool in_str = false;
        for (size_t i = start; i < text.size(); ++i) {
            char c = text[i];
            if (in_str) { if (c == '\\') ++i; else if (c == '"') in_str = false; continue; }
            if (c == '"') in_str = true;
            else if (c == '{') ++depth;
            else if (c == '}' && --depth == 0) {
                try { out = Json::parse(text.substr(start, i - start + 1)); if (out.is_object()) return true; } catch (const Error&) {}
                break;
            }
        }
        ++start;
    }
}

// ---------------------------------------------------------------------------- backends

LlamaServerBackend::LlamaServerBackend(std::string base_url, int idle_timeout_ms) : base_(std::move(base_url)), idle_ms_(idle_timeout_ms) {
    while (!base_.empty() && base_.back() == '/') base_.pop_back();
}

std::string LlamaServerBackend::probe(int* n_ctx, int* slots) const {
    try {
        HttpOptions o; o.connect_timeout_ms = 1500; o.idle_timeout_ms = 3000;
        auto r = http_request("GET", base_ + "/props", {}, "", nullptr, nullptr, o);
        if (r.status != 200) return "the model server answered HTTP " + std::to_string(r.status);
        try {
            Json j = Json::parse(r.body);
            if (n_ctx) *n_ctx = static_cast<int>(j["default_generation_settings"]["n_ctx"].as_int());
            if (slots) *slots = static_cast<int>(j["total_slots"].as_int(1));
        } catch (const Error&) {}
        return "";
    } catch (const Error& e) { return e.what(); }
}

std::string LlamaServerBackend::complete(const LlmRequest& req, const std::function<void(const std::string&)>& on_delta, const std::atomic<bool>& cancel) {
    Json b = Json::object();
    b.set("prompt", req.prompt);
    b.set("n_predict", req.max_tokens);
    b.set("temperature", req.temperature);
    b.set("cache_prompt", true);
    b.set("stream", true);
    if (req.seed >= 0) b.set("seed", req.seed);
    if (req.slot >= 0) b.set("id_slot", req.slot);
    if (!req.schema.is_null()) b.set("json_schema", req.schema);
    if (!req.stop.empty()) { Json s = Json::array(); for (auto& x : req.stop) s.push(x); b.set("stop", s); }

    std::string full, line_buf, error_body;
    bool finished = false;
    HttpOptions o; o.idle_timeout_ms = idle_ms_;
    auto handle_line = [&](const std::string& line) {
        if (line.compare(0, 5, "data:") != 0) return;
        std::string payload = trim(line.substr(5));
        if (payload.empty() || payload == "[DONE]") return;
        Json j;
        try { j = Json::parse(payload); } catch (const Error&) { return; }
        if (j["error"].is_object()) throw Error("the model server reported: " + j["error"]["message"].str_or("an error"));
        const std::string& piece = j["content"].as_str();
        if (!piece.empty()) { full += piece; on_delta(piece); }
        if (j["stop"].as_bool()) finished = true;
    };
    HttpResponse r = http_request("POST", base_ + "/completion", {}, b.dump(), [&](std::string_view d) {
        line_buf.append(d);
        size_t nl;
        while ((nl = line_buf.find('\n')) != std::string::npos) {
            std::string line = line_buf.substr(0, nl);
            line_buf.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            handle_line(line);
        }
        return true;
    }, &cancel, o);
    if (r.status != 200) {
        std::string why = r.body;
        try { why = Json::parse(r.body)["error"]["message"].str_or(r.body); } catch (const Error&) {}
        throw Error("the model server answered HTTP " + std::to_string(r.status) + (why.empty() ? "" : ": " + why.substr(0, 300)));
    }
    if (!line_buf.empty()) handle_line(line_buf);
    (void)finished;
    return full;
}

std::string ScriptedBackend::complete(const LlmRequest& req, const std::function<void(const std::string&)>& on_delta, const std::atomic<bool>& cancel) {
    std::string out;
    size_t idx;
    {
        std::lock_guard<std::mutex> l(m_);
        calls_.push_back({req, false});
        idx = calls_.size() - 1;
        if (queue_.empty()) throw Error("scripted backend has no more answers");
        out = queue_.front();
        queue_.pop_front();
    }
    if (out == "__ERROR__") throw Error("scripted failure");
    for (size_t i = 0; i < out.size(); i += 6) {
        if (cancel.load()) { std::lock_guard<std::mutex> l(m_); calls_[idx].cancelled = true; throw Cancelled(); }
        int d = delay_ms_.load();
        if (d) std::this_thread::sleep_for(std::chrono::milliseconds(d));
        on_delta(out.substr(i, 6));
    }
    return out;
}

// ---------------------------------------------------------------------------- scheduler

LlmScheduler::LlmScheduler(std::shared_ptr<LlmBackend> backend) : backend_(std::move(backend)) {
    worker_ = std::thread([this] { run(); });
}

LlmScheduler::~LlmScheduler() {
    {
        std::lock_guard<std::mutex> l(m_);
        stop_ = true;
        queue_.clear();
    }
    cancel_running_ = true;
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

uint64_t LlmScheduler::submit(LlmRequest req, LlmPriority prio) {
    std::lock_guard<std::mutex> l(m_);
    uint64_t id = next_id_++;
    auto it = queue_.begin();
    while (it != queue_.end() && static_cast<int>(it->prio) <= static_cast<int>(prio)) ++it;   // stable within a priority
    queue_.insert(it, Job{id, std::move(req), prio});
    // A player reply must not wait behind background work that is already running.
    if (running_id_ && prio == LlmPriority::Player && running_prio_ == LlmPriority::Background) cancel_running_ = true;
    cv_.notify_one();
    return id;
}

void LlmScheduler::cancel(uint64_t id) {
    std::lock_guard<std::mutex> l(m_);
    for (auto it = queue_.begin(); it != queue_.end(); ++it)
        if (it->id == id) { events_.push_back({LlmEvent::Cancelled, id, ""}); queue_.erase(it); ev_cv_.notify_all(); return; }
    if (running_id_ == id) cancel_running_ = true;
}

void LlmScheduler::cancel_all(LlmPriority at_or_below) {
    std::lock_guard<std::mutex> l(m_);
    for (auto it = queue_.begin(); it != queue_.end();) {
        if (static_cast<int>(it->prio) >= static_cast<int>(at_or_below)) { events_.push_back({LlmEvent::Cancelled, it->id, ""}); it = queue_.erase(it); }
        else ++it;
    }
    if (running_id_ && static_cast<int>(running_prio_) >= static_cast<int>(at_or_below)) cancel_running_ = true;
    ev_cv_.notify_all();
}

void LlmScheduler::poll(std::vector<LlmEvent>& out) {
    std::lock_guard<std::mutex> l(m_);
    for (auto& e : events_) {
        if (e.type == LlmEvent::Delta) {
            // keep only the newest Delta for a job: it already holds everything generated so far
            bool replaced = false;
            for (auto& o : out) if (o.type == LlmEvent::Delta && o.id == e.id) { o.text = e.text; replaced = true; }
            if (replaced) continue;
        }
        out.push_back(std::move(e));
    }
    events_.clear();
}

bool LlmScheduler::idle() const { std::lock_guard<std::mutex> l(m_); return queue_.empty() && running_id_ == 0; }
size_t LlmScheduler::queued() const { std::lock_guard<std::mutex> l(m_); return queue_.size(); }

bool LlmScheduler::wait(int timeout_ms) {
    std::unique_lock<std::mutex> l(m_);
    return ev_cv_.wait_for(l, std::chrono::milliseconds(timeout_ms), [&] { return !events_.empty(); });
}

void LlmScheduler::run() {
    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> l(m_);
            cv_.wait(l, [&] { return stop_ || !queue_.empty(); });
            if (stop_) return;
            job = std::move(queue_.front());
            queue_.pop_front();
            running_id_ = job.id;
            running_prio_ = job.prio;
            cancel_running_ = false;
        }
        std::string full, acc;
        LlmEvent last{LlmEvent::Done, job.id, ""};
        try {
            full = backend_->complete(job.req, [&](const std::string& piece) {
                acc += piece;
                std::lock_guard<std::mutex> l(m_);
                events_.push_back({LlmEvent::Delta, job.id, acc});
                ev_cv_.notify_all();
            }, cancel_running_);
            last = {LlmEvent::Done, job.id, full};
        } catch (const Cancelled&) {
            last = {LlmEvent::Cancelled, job.id, ""};
        } catch (const std::exception& e) {
            last = {LlmEvent::Failed, job.id, e.what()};
        }
        {
            std::lock_guard<std::mutex> l(m_);
            running_id_ = 0;
            events_.push_back(std::move(last));
            ev_cv_.notify_all();
        }
    }
}

} // namespace sf
