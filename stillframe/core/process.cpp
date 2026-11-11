#include "process.hpp"
#include "platform.hpp"
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace sf {

using Clock = std::chrono::steady_clock;

#ifndef _WIN32
struct Process::Impl {
    pid_t pid = -1;
    int in_fd = -1, out_fd = -1;
    std::string buf;
    bool exited = false;
    int code = 0;
    bool reap(int flags) {
        if (exited) return true;
        int st = 0;
        pid_t r = ::waitpid(pid, &st, flags);
        if (r == pid) { exited = true; code = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0); return true; }
        return false;
    }
};

Process::Process() : p_(new Impl) {}
int Process::pid() const { return p_->pid; }

std::unique_ptr<Process> Process::spawn(const SpawnOptions& opt) {
    if (opt.argv.empty()) throw Error("no program to start");
    int in_pipe[2] = {-1, -1}, out_pipe[2] = {-1, -1}, err_pipe[2];
    if (::pipe(err_pipe) != 0) throw Error("pipe failed");       // reports exec failure to the parent
    ::fcntl(err_pipe[1], F_SETFD, FD_CLOEXEC);
    if (opt.pipe_stdio && (::pipe(in_pipe) != 0 || ::pipe(out_pipe) != 0)) throw Error("pipe failed");
    std::vector<std::string> envs;
    for (char** e = environ; *e; ++e) {
        std::string kv = *e;
        auto eq = kv.find('=');
        if (eq != std::string::npos && opt.env.count(kv.substr(0, eq))) continue;
        envs.push_back(kv);
    }
    for (auto& kv : opt.env) envs.push_back(kv.first + "=" + kv.second);
    std::vector<char*> argv, envp;
    for (auto& a : opt.argv) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    for (auto& e : envs) envp.push_back(const_cast<char*>(e.c_str()));
    envp.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) throw Error("fork failed");
    if (pid == 0) {
        ::setpgid(0, 0);
        if (opt.pipe_stdio) { ::dup2(in_pipe[0], 0); ::dup2(out_pipe[1], 1); }
        else { int nul = ::open("/dev/null", O_RDONLY); ::dup2(nul, 0); }
        int ef = opt.stderr_path.empty() ? ::open("/dev/null", O_WRONLY) : ::open(opt.stderr_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (ef >= 0) { ::dup2(ef, 2); if (!opt.pipe_stdio) ::dup2(ef, 1); }
        for (int fd : {in_pipe[0], in_pipe[1], out_pipe[0], out_pipe[1], err_pipe[0]}) if (fd >= 0) ::close(fd);
        if (!opt.cwd.empty() && ::chdir(opt.cwd.c_str()) != 0) { char c = 'd'; (void)!::write(err_pipe[1], &c, 1); _exit(127); }
        ::execvpe(argv[0], argv.data(), envp.data());
        char c = 'x';
        (void)!::write(err_pipe[1], &c, 1);
        _exit(127);
    }
    ::close(err_pipe[1]);
    if (opt.pipe_stdio) { ::close(in_pipe[0]); ::close(out_pipe[1]); }
    char c;
    ssize_t n = ::read(err_pipe[0], &c, 1);
    ::close(err_pipe[0]);
    if (n > 0) {
        ::waitpid(pid, nullptr, 0);
        if (opt.pipe_stdio) { ::close(in_pipe[1]); ::close(out_pipe[0]); }
        throw Error(c == 'd' ? "cannot enter folder " + opt.cwd : "cannot start '" + opt.argv[0] + "' (is it installed and on PATH?)");
    }
    std::unique_ptr<Process> p(new Process);
    p->p_->pid = pid;
    if (opt.pipe_stdio) { p->p_->in_fd = in_pipe[1]; p->p_->out_fd = out_pipe[0]; ::fcntl(p->p_->out_fd, F_SETFD, FD_CLOEXEC); ::fcntl(p->p_->in_fd, F_SETFD, FD_CLOEXEC); }
    return p;
}

bool Process::write_line(const std::string& line) {
    if (p_->in_fd < 0) return false;
    std::string d = line + "\n";
    size_t off = 0;
    signal(SIGPIPE, SIG_IGN);
    while (off < d.size()) {
        ssize_t n = ::write(p_->in_fd, d.data() + off, d.size() - off);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        off += static_cast<size_t>(n);
    }
    return true;
}

bool Process::read_line(std::string& out, int timeout_ms, bool& eof) {
    eof = false;
    auto t0 = Clock::now();
    while (true) {
        auto nl = p_->buf.find('\n');
        if (nl != std::string::npos) {
            out = p_->buf.substr(0, nl);
            if (!out.empty() && out.back() == '\r') out.pop_back();
            p_->buf.erase(0, nl + 1);
            return true;
        }
        if (p_->out_fd < 0) { eof = true; if (!p_->buf.empty()) { out = p_->buf; p_->buf.clear(); eof = false; return true; } return false; }
        int left = timeout_ms - static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count());
        if (left < 0) left = 0;
        pollfd pfd{p_->out_fd, POLLIN, 0};
        int r = ::poll(&pfd, 1, left);
        if (r == 0) return false;
        if (r < 0) { if (errno == EINTR) continue; eof = true; return false; }
        char tmp[4096];
        ssize_t n = ::read(p_->out_fd, tmp, sizeof tmp);
        if (n > 0) p_->buf.append(tmp, static_cast<size_t>(n));
        else if (n == 0) { ::close(p_->out_fd); p_->out_fd = -1; }
        else if (errno != EINTR && errno != EAGAIN) { ::close(p_->out_fd); p_->out_fd = -1; }
    }
}

bool Process::running() { return !p_->reap(WNOHANG); }

int Process::wait(int timeout_ms) {
    auto t0 = Clock::now();
    while (!p_->reap(WNOHANG)) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count() >= timeout_ms) return -9999;
        sleep_ms(10);
    }
    return p_->code;
}

void Process::close_stdin() { if (p_->in_fd >= 0) { ::close(p_->in_fd); p_->in_fd = -1; } }

void Process::terminate() {
    if (p_->exited) return;
    ::kill(-p_->pid, SIGTERM);   // the whole process group
    if (wait(1500) == -9999) { ::kill(-p_->pid, SIGKILL); wait(2000); }
}

Process::~Process() {
    if (!p_) return;
    terminate();
    if (p_->in_fd >= 0) ::close(p_->in_fd);
    if (p_->out_fd >= 0) ::close(p_->out_fd);
}

#else // ---------------------------------------------------------------- Windows
struct Process::Impl {
    HANDLE proc = nullptr, thread = nullptr, in_w = nullptr, out_r = nullptr, job = nullptr;
    DWORD pid = 0;
    std::string buf;
    bool exited = false;
    int code = 0;
};

Process::Process() : p_(new Impl) {}
int Process::pid() const { return static_cast<int>(p_->pid); }

static std::string quote_arg(const std::string& a) {
    if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos) return a;
    std::string r = "\"";
    size_t bs = 0;
    for (char c : a) {
        if (c == '\\') { ++bs; continue; }
        if (c == '"') { r.append(bs * 2 + 1, '\\'); r += '"'; bs = 0; continue; }
        r.append(bs, '\\'); bs = 0; r += c;
    }
    r.append(bs * 2, '\\');
    return r + "\"";
}
static std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::unique_ptr<Process> Process::spawn(const SpawnOptions& opt) {
    if (opt.argv.empty()) throw Error("no program to start");
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE in_r = nullptr, in_w = nullptr, out_r = nullptr, out_w = nullptr;
    if (opt.pipe_stdio) {
        if (!CreatePipe(&in_r, &in_w, &sa, 0) || !CreatePipe(&out_r, &out_w, &sa, 0)) throw Error("pipe failed");
        SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    }
    HANDLE err_h = CreateFileW(widen(opt.stderr_path.empty() ? "NUL" : opt.stderr_path).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, opt.stderr_path.empty() ? OPEN_EXISTING : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE nul_in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = opt.pipe_stdio ? in_r : nul_in;
    si.hStdOutput = opt.pipe_stdio ? out_w : err_h;
    si.hStdError = err_h;
    std::string cmd;
    for (size_t i = 0; i < opt.argv.size(); ++i) { if (i) cmd += ' '; cmd += quote_arg(opt.argv[i]); }
    std::wstring wcmd = widen(cmd);
    std::wstring wcwd = widen(opt.cwd);
    std::wstring envblock;
    if (!opt.env.empty()) {
        LPWCH cur = GetEnvironmentStringsW();
        for (LPWCH p = cur; *p; p += wcslen(p) + 1) {
            std::wstring kv = p;
            auto eq = kv.find(L'=', 1);
            bool replaced = false;
            if (eq != std::wstring::npos) for (auto& e : opt.env) if (_wcsicmp(kv.substr(0, eq).c_str(), widen(e.first).c_str()) == 0) replaced = true;
            if (!replaced) { envblock += kv; envblock += L'\0'; }
        }
        FreeEnvironmentStringsW(cur);
        for (auto& e : opt.env) { envblock += widen(e.first + "=" + e.second); envblock += L'\0'; }
        envblock += L'\0';
    }
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
                             envblock.empty() ? nullptr : envblock.data(), wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi);
    if (opt.pipe_stdio) { CloseHandle(in_r); CloseHandle(out_w); }
    if (err_h != INVALID_HANDLE_VALUE) CloseHandle(err_h);
    if (nul_in != INVALID_HANDLE_VALUE) CloseHandle(nul_in);
    if (!ok) {
        if (opt.pipe_stdio) { CloseHandle(in_w); CloseHandle(out_r); }
        throw Error("cannot start '" + opt.argv[0] + "' (is it installed and on PATH?)");
    }
    std::unique_ptr<Process> p(new Process);
    // A job object makes sure children die with the engine and lets us kill their whole tree.
    p->p_->job = CreateJobObjectW(nullptr, nullptr);
    if (p->p_->job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(p->p_->job, JobObjectExtendedLimitInformation, &li, sizeof li);
        AssignProcessToJobObject(p->p_->job, pi.hProcess);
    }
    ResumeThread(pi.hThread);
    p->p_->proc = pi.hProcess; p->p_->thread = pi.hThread; p->p_->pid = pi.dwProcessId;
    p->p_->in_w = in_w; p->p_->out_r = out_r;
    return p;
}

bool Process::write_line(const std::string& line) {
    if (!p_->in_w) return false;
    std::string d = line + "\n";
    DWORD w = 0;
    return WriteFile(p_->in_w, d.data(), static_cast<DWORD>(d.size()), &w, nullptr) && w == d.size();
}

bool Process::read_line(std::string& out, int timeout_ms, bool& eof) {
    eof = false;
    auto t0 = Clock::now();
    while (true) {
        auto nl = p_->buf.find('\n');
        if (nl != std::string::npos) {
            out = p_->buf.substr(0, nl);
            if (!out.empty() && out.back() == '\r') out.pop_back();
            p_->buf.erase(0, nl + 1);
            return true;
        }
        if (!p_->out_r) { eof = true; if (!p_->buf.empty()) { out = p_->buf; p_->buf.clear(); eof = false; return true; } return false; }
        DWORD avail = 0;
        if (!PeekNamedPipe(p_->out_r, nullptr, 0, nullptr, &avail, nullptr)) { CloseHandle(p_->out_r); p_->out_r = nullptr; continue; }
        if (avail) {
            char tmp[4096]; DWORD n = 0;
            if (ReadFile(p_->out_r, tmp, sizeof tmp, &n, nullptr) && n) p_->buf.append(tmp, n);
            continue;
        }
        if (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count() >= timeout_ms) return false;
        Sleep(5);
    }
}

bool Process::running() {
    if (p_->exited) return false;
    DWORD code = 0;
    if (GetExitCodeProcess(p_->proc, &code) && code != STILL_ACTIVE) { p_->exited = true; p_->code = static_cast<int>(code); return false; }
    return true;
}
int Process::wait(int timeout_ms) {
    auto t0 = Clock::now();
    while (running()) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count() >= timeout_ms) return -9999;
        Sleep(10);
    }
    return p_->code;
}
void Process::close_stdin() { if (p_->in_w) { CloseHandle(p_->in_w); p_->in_w = nullptr; } }
void Process::terminate() {
    if (!running()) return;
    if (p_->job) TerminateJobObject(p_->job, 1); else TerminateProcess(p_->proc, 1);
    wait(2000);
}
Process::~Process() {
    if (!p_) return;
    terminate();
    if (p_->in_w) CloseHandle(p_->in_w);
    if (p_->out_r) CloseHandle(p_->out_r);
    if (p_->thread) CloseHandle(p_->thread);
    if (p_->proc) CloseHandle(p_->proc);
    if (p_->job) CloseHandle(p_->job);
}
#endif

} // namespace sf
