#include "json.hpp"
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace sf {

static const Json kNull;
static const std::string kEmpty;
static const Json::Array kNoItems;
static const Json::Object kNoMembers;

bool Json::as_bool(bool def) const { return is_bool() ? std::get<bool>(v_) : def; }
int64_t Json::as_int(int64_t def) const {
    if (is_int()) return std::get<int64_t>(v_);
    if (type() == Type::Real) return static_cast<int64_t>(std::get<double>(v_));
    return def;
}
double Json::as_num(double def) const {
    if (is_int()) return static_cast<double>(std::get<int64_t>(v_));
    if (type() == Type::Real) return std::get<double>(v_);
    return def;
}
const std::string& Json::as_str() const { return is_string() ? std::get<std::string>(v_) : kEmpty; }
const Json::Array& Json::items() const { return is_array() ? std::get<Array>(v_) : kNoItems; }
Json::Array& Json::items_mut() {
    if (is_null()) v_ = Array{};
    if (!is_array()) throw Error("json: value is not an array");
    return std::get<Array>(v_);
}
const Json::Object& Json::members() const { return is_object() ? std::get<Object>(v_) : kNoMembers; }
size_t Json::size() const {
    if (is_array()) return std::get<Array>(v_).size();
    if (is_object()) return std::get<Object>(v_).size();
    return 0;
}
const Json* Json::find(std::string_view key) const {
    if (!is_object()) return nullptr;
    for (auto& kv : std::get<Object>(v_)) if (kv.first == key) return &kv.second;
    return nullptr;
}
Json* Json::find(std::string_view key) {
    if (!is_object()) return nullptr;
    for (auto& kv : std::get<Object>(v_)) if (kv.first == key) return &kv.second;
    return nullptr;
}
const Json& Json::operator[](std::string_view key) const { auto p = find(key); return p ? *p : kNull; }
const Json& Json::at(size_t i) const { auto& a = items(); return i < a.size() ? a[i] : kNull; }
Json& Json::set(std::string key, Json v) {
    if (is_null()) v_ = Object{};
    if (!is_object()) throw Error("json: value is not an object");
    auto& o = std::get<Object>(v_);
    for (auto& kv : o) if (kv.first == key) { kv.second = std::move(v); return kv.second; }
    o.emplace_back(std::move(key), std::move(v));
    return o.back().second;
}
Json& Json::push(Json v) {
    auto& a = items_mut();
    a.push_back(std::move(v));
    return a.back();
}
bool Json::erase(std::string_view key) {
    if (!is_object()) return false;
    auto& o = std::get<Object>(v_);
    for (size_t i = 0; i < o.size(); ++i) if (o[i].first == key) { o.erase(o.begin() + i); return true; }
    return false;
}
bool Json::operator==(const Json& o) const {
    if (is_number() && o.is_number()) return as_num() == o.as_num();
    if (type() != o.type()) return false;
    switch (type()) {
        case Type::Null: return true;
        case Type::Bool: return as_bool() == o.as_bool();
        case Type::String: return as_str() == o.as_str();
        case Type::Array: return items() == o.items();
        case Type::Object: {
            auto &a = members(), &b = o.members();
            if (a.size() != b.size()) return false;
            for (auto& kv : a) { auto p = o.find(kv.first); if (!p || *p != kv.second) return false; }
            return true;
        }
        default: return true;
    }
}

// ---------------------------------------------------------------- writer
static void escape(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                else out += static_cast<char>(c);
        }
    }
    out += '"';
}

void Json::write(std::string& out, int indent, int depth) const {
    auto nl = [&](int d) { if (indent >= 0) { out += '\n'; out.append(static_cast<size_t>(indent) * d, ' '); } };
    switch (type()) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += as_bool() ? "true" : "false"; break;
        case Type::Int: out += std::to_string(std::get<int64_t>(v_)); break;
        case Type::Real: {
            double d = std::get<double>(v_);
            if (!std::isfinite(d)) { out += "null"; break; }
            char buf[40];
            auto r = std::to_chars(buf, buf + sizeof buf, d);
            std::string t(buf, r.ptr);
            if (t.find_first_of(".eEn") == std::string::npos) t += ".0";
            out += t;
            break;
        }
        case Type::String: escape(out, as_str()); break;
        case Type::Array: {
            auto& a = items();
            if (a.empty()) { out += "[]"; break; }
            out += '[';
            for (size_t i = 0; i < a.size(); ++i) {
                if (i) out += ',';
                nl(depth + 1);
                a[i].write(out, indent, depth + 1);
            }
            nl(depth);
            out += ']';
            break;
        }
        case Type::Object: {
            auto& o = members();
            if (o.empty()) { out += "{}"; break; }
            out += '{';
            for (size_t i = 0; i < o.size(); ++i) {
                if (i) out += ',';
                nl(depth + 1);
                escape(out, o[i].first);
                out += indent >= 0 ? ": " : ":";
                o[i].second.write(out, indent, depth + 1);
            }
            nl(depth);
            out += '}';
            break;
        }
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    write(out, indent, 0);
    return out;
}

// ---------------------------------------------------------------- parser
namespace {
struct Parser {
    std::string_view s;
    size_t i = 0;

    [[noreturn]] void fail(const std::string& what) {
        size_t line = 1, col = 1;
        for (size_t k = 0; k < i && k < s.size(); ++k) { if (s[k] == '\n') { ++line; col = 1; } else ++col; }
        throw Error("json: " + what + " at line " + std::to_string(line) + ", column " + std::to_string(col));
    }
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }
    bool eat(char c) { ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }

    static void utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    uint32_t hex4() {
        if (i + 4 > s.size()) fail("truncated \\u escape");
        uint32_t v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else fail("bad hex digit");
        }
        return v;
    }
    std::string str() {
        std::string out;
        while (true) {
            if (i >= s.size()) fail("unterminated string");
            char c = s[i++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) fail("bad escape");
            char e = s[i++];
            switch (e) {
                case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break;
                case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break;
                case 'r': out += '\r'; break; case 't': out += '\t'; break;
                case 'u': {
                    uint32_t cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00) {
                        if (i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                            i += 2;
                            uint32_t lo = hex4();
                            if (lo < 0xDC00 || lo > 0xDFFF) fail("bad surrogate pair");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else fail("lone surrogate");
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) fail("lone surrogate");
                    utf8(out, cp);
                    break;
                }
                default: fail("bad escape");
            }
        }
    }
    Json number() {
        size_t st = i;
        if (i < s.size() && s[i] == '-') ++i;
        if (i >= s.size() || !(s[i] >= '0' && s[i] <= '9')) fail("bad number");
        if (s[i] == '0' && i + 1 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '9') fail("leading zero");
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        bool real = false;
        if (i < s.size() && s[i] == '.') {
            real = true; ++i;
            if (i >= s.size() || !(s[i] >= '0' && s[i] <= '9')) fail("bad fraction");
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            real = true; ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            if (i >= s.size() || !(s[i] >= '0' && s[i] <= '9')) fail("bad exponent");
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        }
        std::string t(s.substr(st, i - st));
        if (!real) {
            int64_t v = 0;
            auto r = std::from_chars(t.data(), t.data() + t.size(), v);
            if (r.ec == std::errc()) return Json(v);
        }
        return Json(std::strtod(t.c_str(), nullptr));
    }
    Json value(int depth) {
        if (depth > 200) fail("nesting too deep");
        ws();
        if (i >= s.size()) fail("unexpected end");
        char c = s[i];
        if (c == '{') {
            ++i;
            Json o = Json::object();
            if (eat('}')) return o;
            do {
                ws();
                if (i >= s.size() || s[i] != '"') fail("expected object key");
                ++i;
                std::string k = str();
                if (!eat(':')) fail("expected ':'");
                o.set(std::move(k), value(depth + 1));
            } while (eat(','));
            if (!eat('}')) fail("expected '}'");
            return o;
        }
        if (c == '[') {
            ++i;
            Json a = Json::array();
            if (eat(']')) return a;
            do a.push(value(depth + 1)); while (eat(','));
            if (!eat(']')) fail("expected ']'");
            return a;
        }
        if (c == '"') { ++i; return Json(str()); }
        if (s.substr(i, 4) == "true") { i += 4; return Json(true); }
        if (s.substr(i, 5) == "false") { i += 5; return Json(false); }
        if (s.substr(i, 4) == "null") { i += 4; return Json(); }
        return number();
    }
};
} // namespace

Json Json::parse(std::string_view text) {
    if (text.size() >= 3 && static_cast<uint8_t>(text[0]) == 0xEF && static_cast<uint8_t>(text[1]) == 0xBB && static_cast<uint8_t>(text[2]) == 0xBF)
        text.remove_prefix(3);
    Parser p{text};
    Json v = p.value(0);
    p.ws();
    if (p.i != text.size()) p.fail("trailing characters");
    return v;
}

} // namespace sf
