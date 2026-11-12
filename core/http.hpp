// Minimal HTTP/1.1 client for talking to a local model server (plain http, streaming, cancellable).
#pragma once
#include "common.hpp"
#include <atomic>
#include <functional>
#include <map>

namespace sf {

struct HttpResponse {
    int status = 0;
    std::string body;                             // empty when streamed to on_data (except error bodies)
    std::map<std::string, std::string> headers;   // lower-case names
};

struct HttpOptions {
    int connect_timeout_ms = 3000;
    int idle_timeout_ms = 600000;     // no bytes received for this long => fail
};

// Sends a request. If `on_data` is set, decoded body bytes are delivered as they arrive (return false to abort).
// `cancel` is polled about every 50 ms. Throws Error on connection problems, timeouts and cancellation.
HttpResponse http_request(const std::string& method, const std::string& url,
                          const std::vector<std::pair<std::string, std::string>>& headers,
                          const std::string& body,
                          const std::function<bool(std::string_view)>& on_data = nullptr,
                          const std::atomic<bool>* cancel = nullptr,
                          const HttpOptions& opt = {});

struct Cancelled : Error { Cancelled() : Error("cancelled") {} };

// "http://host:port/path" -> parts. Only http:// is supported (the model server runs locally).
struct ParsedUrl { std::string host, path; int port; };
ParsedUrl parse_url(const std::string& url);

} // namespace sf
