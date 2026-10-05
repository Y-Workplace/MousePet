#pragma once
#include <cerrno>
#include <fcntl.h>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

namespace mousepet {
inline std::string executablePath() {
    std::vector<char> buf(4096);
    const ssize_t n = readlink("/proc/self/exe", buf.data(), buf.size());
    return n > 0 ? std::string(buf.data(), size_t(n)) : std::string{};
}
inline std::string shellQuote(const std::string& value) {
    std::string out = "'";
    for (char c : value) out += c == '\'' ? "'\\''" : std::string(1, c);
    return out + "'";
}
inline int runProcess(std::vector<std::string> args, bool quiet = false) {
    std::vector<char*> argv;
    for (auto& arg : args) argv.push_back(arg.data());
    argv.push_back(nullptr);
    const pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        if (quiet) {
            const int fd = open("/dev/null", O_WRONLY);
            if (fd >= 0) { dup2(fd, STDOUT_FILENO); dup2(fd, STDERR_FILENO); close(fd); }
        }
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) return 1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
}
