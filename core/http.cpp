#include "http.hpp"
#include <chrono>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using sock_t = SOCKET;
static const sock_t kBad = INVALID_SOCKET;
static void close_sock(sock_t s) { closesocket(s); }
static void set_nonblocking(sock_t s, bool nb) { u_long m = nb ? 1 : 0; ioctlsocket(s, FIONBIO, &m); }
static bool would_block() { int e = WSAGetLastError(); return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
static void net_init() { static std::once_flag f; std::call_once(f, [] { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }); }
#else
#include <arpa/inet.h>
#include <csignal>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_t = int;
static const sock_t kBad = -1;
static void close_sock(sock_t s) { ::close(s); }
static void set_nonblocking(sock_t s, bool nb) { int f = fcntl(s, F_GETFL, 0); fcntl(s, F_SETFL, nb ? (f | O_NONBLOCK) : (f & ~O_NONBLOCK)); }
static bool would_block() { return errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN; }
static void net_init() { signal(SIGPIPE, SIG_IGN); }
#endif

namespace sf {

ParsedUrl parse_url(const std::string& url) {
    if (!starts_with(url, "http://")) throw Error("only http:// URLs are supported: " + url);
    std::string rest = url.substr(7);
    auto slash = rest.find('/');
    std::string hostport = rest.substr(0, slash);
    ParsedUrl u;
    u.path = slash == std::string::npos ? "/" : rest.substr(slash);
    auto colon = hostport.rfind(':');
    if (colon == std::string::npos) { u.host = hostport; u.port = 80; }
    else { u.host = hostport.substr(0, colon); u.port = std::atoi(hostport.c_str() + colon + 1); }
    if (u.host.empty() || u.port <= 0 || u.port > 65535) throw Error("bad URL: " + url);
    return u;
}

namespace {
using Clock = std::chrono::steady_clock;
int ms_since(Clock::time_point t) { return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count()); }

// Waits until the socket is readable/writable. Returns false on timeout slice (caller loops).
bool wait_fd(sock_t s, bool write, int ms) {
    fd_set set; FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    int r = select(static_cast<int>(s) + 1, write ? nullptr : &set, write ? &set : nullptr, nullptr, &tv);
    return r > 0;
}

sock_t connect_to(const ParsedUrl& u, int timeout_ms, const std::atomic<bool>* cancel) {
    net_init();
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(u.host.c_str(), std::to_string(u.port).c_str(), &hints, &res) != 0 || !res) throw Error("cannot resolve " + u.host);
    std::string last = "connection failed";
    for (addrinfo* a = res; a; a = a->ai_next) {
        sock_t s = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == kBad) continue;
        set_nonblocking(s, true);
        int r = ::connect(s, a->ai_addr, static_cast<int>(a->ai_addrlen));
        bool ok = r == 0;
        if (!ok && would_block()) {
            auto t0 = Clock::now();
            while (ms_since(t0) < timeout_ms) {
                if (cancel && cancel->load()) { close_sock(s); freeaddrinfo(res); throw Cancelled(); }
                if (wait_fd(s, true, 50)) {
                    int err = 0; socklen_t len = sizeof err;
                    getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
                    ok = err == 0;
                    if (!ok) last = std::strerror(err);
                    break;
                }
            }
            if (!ok && last == "connection failed") last = "timed out";
        }
        if (ok) {
            freeaddrinfo(res);
            int one = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
            return s;
        }
        close_sock(s);
    }
    freeaddrinfo(res);
    throw Error("cannot connect to " + u.host + ":" + std::to_string(u.port) + " (" + last + ")");
}

struct Conn {
    sock_t s;
    ~Conn() { if (s != kBad) close_sock(s); }
};
} // namespace

HttpResponse http_request(const std::string& method, const std::string& url,
                          const std::vector<std::pair<std::string, std::string>>& headers, const std::string& body,
                          const std::function<bool(std::string_view)>& on_data, const std::atomic<bool>* cancel, const HttpOptions& opt) {
    ParsedUrl u = parse_url(url);
    Conn c{connect_to(u, opt.connect_timeout_ms, cancel)};

    std::string req = method + " " + u.path + " HTTP/1.1\r\nHost: " + u.host + ":" + std::to_string(u.port) + "\r\nConnection: close\r\nAccept: */*\r\n";
    bool has_ct = false;
    for (auto& h : headers) { req += h.first + ": " + h.second + "\r\n"; if (lower(h.first) == "content-type") has_ct = true; }
    if (!body.empty() || method == "POST") {
        if (!has_ct) req += "Content-Type: application/json\r\n";
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    req += "\r\n" + body;

    size_t sent = 0;
    auto t_send = Clock::now();
    while (sent < req.size()) {
        if (cancel && cancel->load()) throw Cancelled();
        if (ms_since(t_send) > opt.idle_timeout_ms) throw Error("timed out sending request");
        if (!wait_fd(c.s, true, 50)) continue;
        int n = ::send(c.s, req.data() + sent, static_cast<int>(req.size() - sent), 0);
        if (n < 0) { if (would_block()) continue; throw Error("connection lost while sending"); }
        sent += static_cast<size_t>(n);
    }

    HttpResponse resp;
    std::string buf;            // raw bytes not yet consumed
    bool headers_done = false, chunked = false, have_len = false, finished = false;
    size_t content_len = 0, got = 0;
    size_t chunk_left = 0;
    enum { ChunkSize, ChunkData, ChunkCrlf, ChunkTrailer } cstate = ChunkSize;
    bool abort_by_user = false;

    auto deliver = [&](std::string_view d) {
        if (d.empty()) return;
        if (on_data && resp.status == 200) {
            if (!on_data(d)) abort_by_user = true;
        } else if (resp.body.size() < (1u << 20) || !on_data) resp.body.append(d);
    };

    auto process = [&]() {
        while (!finished && !abort_by_user) {
            if (!headers_done) {
                auto e = buf.find("\r\n\r\n");
                if (e == std::string::npos) return;
                std::string head = buf.substr(0, e);
                buf.erase(0, e + 4);
                auto line_end = head.find("\r\n");
                std::string status_line = head.substr(0, line_end);
                if (status_line.size() < 12 || status_line.compare(0, 5, "HTTP/") != 0) throw Error("bad HTTP response");
                resp.status = std::atoi(status_line.c_str() + 9);
                size_t p = line_end == std::string::npos ? head.size() : line_end + 2;
                while (p < head.size()) {
                    auto q = head.find("\r\n", p);
                    std::string line = head.substr(p, q == std::string::npos ? std::string::npos : q - p);
                    p = q == std::string::npos ? head.size() : q + 2;
                    auto colon = line.find(':');
                    if (colon != std::string::npos) resp.headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
                }
                chunked = lower(resp.headers["transfer-encoding"]).find("chunked") != std::string::npos;
                if (resp.headers.count("content-length")) { have_len = true; content_len = static_cast<size_t>(std::strtoull(resp.headers["content-length"].c_str(), nullptr, 10)); }
                headers_done = true;
                if (resp.status == 204 || resp.status == 304 || (resp.status >= 100 && resp.status < 200) || method == "HEAD") { finished = true; return; }
            }
            if (chunked) {
                if (cstate == ChunkSize) {
                    auto e = buf.find("\r\n");
                    if (e == std::string::npos) return;
                    chunk_left = static_cast<size_t>(std::strtoull(buf.substr(0, e).c_str(), nullptr, 16));
                    buf.erase(0, e + 2);
                    cstate = chunk_left == 0 ? ChunkTrailer : ChunkData;
                } else if (cstate == ChunkData) {
                    if (buf.empty()) return;
                    size_t n = std::min(chunk_left, buf.size());
                    deliver(std::string_view(buf).substr(0, n));
                    buf.erase(0, n);
                    chunk_left -= n;
                    if (chunk_left == 0) cstate = ChunkCrlf;
                } else if (cstate == ChunkCrlf) {
                    if (buf.size() < 2) return;
                    buf.erase(0, 2);
                    cstate = ChunkSize;
                } else { // trailer: ends with an empty line
                    auto e = buf.find("\r\n");
                    if (e == std::string::npos) return;
                    if (e == 0) { finished = true; return; }
                    buf.erase(0, e + 2);
                }
            } else {
                size_t n = buf.size();
                if (have_len) n = std::min(n, content_len - got);
                if (n) { deliver(std::string_view(buf).substr(0, n)); buf.erase(0, n); got += n; }
                if (have_len && got >= content_len) { finished = true; return; }
                if (buf.empty()) return;
            }
        }
    };

    auto last_rx = Clock::now();
    char tmp[16384];
    while (!finished && !abort_by_user) {
        if (cancel && cancel->load()) throw Cancelled();
        if (ms_since(last_rx) > opt.idle_timeout_ms) throw Error("the model server stopped responding (timeout)");
        if (!wait_fd(c.s, false, 50)) continue;
        int n = ::recv(c.s, tmp, sizeof tmp, 0);
        if (n < 0) { if (would_block()) continue; throw Error("connection lost"); }
        if (n == 0) {
            process();
            if (!finished && !abort_by_user) {
                if (!headers_done) throw Error("the server closed the connection without answering");
                if (chunked || have_len) { if (chunked && cstate != ChunkSize) throw Error("response cut off"); if (have_len && got < content_len) throw Error("response cut off"); }
            }
            break;
        }
        last_rx = Clock::now();
        buf.append(tmp, static_cast<size_t>(n));
        process();
    }
    return resp;
}

} // namespace sf
