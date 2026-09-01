#include "RealJournalProvider.h"
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>

namespace polaris::providers::real {

std::optional<std::string> RealJournalProvider::safeExecGet(const std::string& exe, const std::vector<std::string>& args, int timeoutSec) {
    if (exe.empty() || exe[0] != '/') return std::nullopt;
    if (access(exe.c_str(), X_OK) != 0) return std::nullopt;
    int pipefd[2];
    if (pipe(pipefd)!=0) return std::nullopt;
    pid_t pid=fork();
    if(pid<0){ close(pipefd[0]); close(pipefd[1]); return std::nullopt; }
    if(pid==0){
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for(auto &a: args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(exe.c_str(), argv.data());
        _exit(127);
    }
    close(pipefd[1]);
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    std::string out;
    char buf[4096];
    struct pollfd pfd{pipefd[0], POLLIN, 0};
    int elapsed=0;
    while(elapsed < timeoutSec*1000){
        int r=poll(&pfd,1,200);
        if(r>0 && (pfd.revents & POLLIN)){
            ssize_t n=read(pipefd[0],buf,sizeof(buf));
            if(n>0) out.append(buf,n);
            else if(n==0) break;
        }
        int status; pid_t w=waitpid(pid,&status,WNOHANG);
        if(w==pid) break;
        elapsed+=200;
    }
    int status;
    if(waitpid(pid,&status,WNOHANG)==0){
        kill(pid,SIGTERM);
        poll(nullptr,0,500);
        waitpid(pid,&status,0);
        close(pipefd[0]);
        return std::nullopt;
    }
    close(pipefd[0]);
    return out;
}

} // namespace polaris::providers::real
