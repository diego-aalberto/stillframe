// Talking to a language model without ever blocking the game.
//
//   LlmBackend      one blocking, streaming, cancellable completion (llama-server, or a scripted fake)
//   LlmScheduler    runs requests on a worker thread in priority order, so the player's reply is never
//                   stuck behind an interjection, a summary or voice synthesis. The game thread only
//                   ever calls submit()/cancel()/poll().
//   PartialReply    pulls the visible "reply" text out of half-written JSON, so it can be typed out
//                   while the model is still generating.
#pragma once
#include "json.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace sf {

struct LlmRequest {
    std::string prompt;
    Json schema;                   // JSON schema the output must follow; null = unconstrained
    int max_tokens = 400;
    double temperature = 0.8;
    int64_t seed = -1;
    int slot = -1;                 // llama-server slot, so one character keeps its KV cache
    std::vector<std::string> stop;
};

class LlmBackend {
public:
    virtual ~LlmBackend() = default;
    // Returns the full text. Calls on_delta with each new piece. Throws Cancelled when `cancel` is set.
    virtual std::string complete(const LlmRequest& req, const std::function<void(const std::string&)>& on_delta,
                                 const std::atomic<bool>& cancel) = 0;
    virtual std::string describe() const = 0;
};

// llama.cpp's llama-server: POST /completion, server-sent events.
class LlamaServerBackend : public LlmBackend {
public:
    explicit LlamaServerBackend(std::string base_url, int idle_timeout_ms = 120000);
    std::string complete(const LlmRequest&, const std::function<void(const std::string&)>&, const std::atomic<bool>&) override;
    std::string describe() const override { return "llama-server at " + base_; }
    // Checks /props. Returns an empty string when reachable, else a readable reason.
    std::string probe(int* n_ctx = nullptr, int* slots = nullptr) const;
private:
    std::string base_;
    int idle_ms_;
};

// For tests and for running a game with no model: replays prepared answers in order.
class ScriptedBackend : public LlmBackend {
public:
    struct Call { LlmRequest req; bool cancelled = false; };
    void push(std::string text) { std::lock_guard<std::mutex> l(m_); queue_.push_back(std::move(text)); }
    void set_delay_ms(int ms) { delay_ms_ = ms; }
    std::vector<Call> calls() const { std::lock_guard<std::mutex> l(m_); return calls_; }
    size_t pending() const { std::lock_guard<std::mutex> l(m_); return queue_.size(); }
    std::string complete(const LlmRequest&, const std::function<void(const std::string&)>&, const std::atomic<bool>&) override;
    std::string describe() const override { return "scripted backend"; }
private:
    mutable std::mutex m_;
    std::deque<std::string> queue_;
    std::vector<Call> calls_;
    std::atomic<int> delay_ms_{0};
};

enum class LlmPriority { Player = 0, Interject = 1, Background = 2 };   // lower number runs first

struct LlmEvent {
    enum Type { Delta, Done, Failed, Cancelled } type;
    uint64_t id = 0;
    std::string text;      // Delta/Done: everything generated so far; Failed: the reason
};

class LlmScheduler {
public:
    explicit LlmScheduler(std::shared_ptr<LlmBackend> backend);
    ~LlmScheduler();
    uint64_t submit(LlmRequest req, LlmPriority prio);
    void cancel(uint64_t id);                       // queued: dropped; running: aborted mid-stream
    void cancel_all(LlmPriority at_or_below = LlmPriority::Background);
    // Moves finished/streaming events to `out`. Delta events for one job are merged into the latest.
    void poll(std::vector<LlmEvent>& out);
    bool idle() const;                              // nothing queued or running
    size_t queued() const;
    // Blocks until an event is available or the timeout passes. Mostly for tests and headless tools.
    bool wait(int timeout_ms);
    LlmBackend& backend() { return *backend_; }
private:
    struct Job { uint64_t id; LlmRequest req; LlmPriority prio; };
    void run();
    std::shared_ptr<LlmBackend> backend_;
    mutable std::mutex m_;
    std::condition_variable cv_, ev_cv_;
    std::deque<Job> queue_;
    std::vector<LlmEvent> events_;
    uint64_t next_id_ = 1, running_id_ = 0;
    LlmPriority running_prio_ = LlmPriority::Background;
    std::atomic<bool> cancel_running_{false};
    bool stop_ = false;
    std::thread worker_;
};

// ---- partial JSON helpers --------------------------------------------------------------------

// The text of the top-level string field `key` as far as it has been written (escapes decoded).
// `closed` is set when the closing quote has been seen.
std::string partial_string_field(std::string_view partial, std::string_view key, bool* closed = nullptr);

// The first complete top-level JSON object inside `text` (models sometimes add chatter around it).
// Returns false when there isn't one.
bool extract_json_object(std::string_view text, Json& out);

} // namespace sf
