#include "RealGpuProvider.h"
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <vector>
#include <string>
#include <optional>

namespace polaris::providers::real {

std::optional<std::string> RealGpuProvider::safeExecGetOutput(const std::string& exe, const std::vector<std::string>& args, int timeoutSec) {
    // Validate fixed executable path (no shell, no user input concat)
    if (exe.empty() || exe[0] != '/') return std::nullopt;
    // Check executable exists and is file
    if (access(exe.c_str(), X_OK) != 0) return std::nullopt;
    int pipefd[2];
    if (pipe(pipefd) != 0) return std::nullopt;
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return std::nullopt; }
    if (pid == 0) {
        // Child: close read, dup write to stdout+stderr
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (auto &a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(exe.c_str(), argv.data());
        _exit(127);
    }
    // Parent
    close(pipefd[1]);
    // Set non-blocking
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    std::string out;
    char buf[1024];
    struct pollfd pfd{pipefd[0], POLLIN, 0};
    int elapsed=0;
    while (elapsed < timeoutSec*1000) {
        int r = poll(&pfd, 1, 200);
        if (r > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = read(pipefd[0], buf, sizeof(buf));
            if (n > 0) out.append(buf, n);
            else if (n==0) break;
        } else if (r==0) { /* timeout tick */ }
        else break;
        // check child still alive
        int status;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) break;
        elapsed += 200;
    }
    // Kill if still running beyond timeout
    int status;
    if (waitpid(pid, &status, WNOHANG)==0) {
        // timeout
        kill(pid, SIGTERM);
        // give 500ms
        poll(nullptr,0,500);
        waitpid(pid, &status, 0);
        close(pipefd[0]);
        return std::nullopt; // timeout -> treat as unavailable
    }
    close(pipefd[0]);
    if (out.empty()) return std::nullopt;
    return out;
}

} // namespace polaris::providers::real
