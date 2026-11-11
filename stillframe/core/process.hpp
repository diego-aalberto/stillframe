// Child processes with line-oriented stdin/stdout. Used for minigames and for launching the model server.
#pragma once
#include "common.hpp"
#include <map>
#include <memory>

namespace sf {

struct SpawnOptions {
    std::vector<std::string> argv;              // argv[0] is the program
    std::string cwd;                            // empty = inherit
    std::map<std::string, std::string> env;     // added to the inherited environment
    std::string stderr_path;                    // file to append stderr to; empty = discard
    bool pipe_stdio = true;                     // false: no stdin/stdout pipes (stdout/stderr go to stderr_path)
};

class Process {
public:
    static std::unique_ptr<Process> spawn(const SpawnOptions& opt);   // throws Error if it cannot start
    ~Process();                                                       // terminates a still-running child

    bool write_line(const std::string& line);                         // false if the pipe is closed
    // Waits up to timeout_ms for a full line. Returns true and fills `out` on success; sets `eof` when the
    // child closed stdout and nothing more can be read.
    bool read_line(std::string& out, int timeout_ms, bool& eof);
    bool running();
    // Exit code if finished within timeout_ms, else -9999.
    int wait(int timeout_ms);
    void terminate();                                                 // polite request, then forced kill
    void close_stdin();
    int pid() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
    Process();
};

} // namespace sf
