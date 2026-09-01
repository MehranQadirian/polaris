#pragma once
#include "../../domain/SystemInfo.h"
#include "../../safety/ReadOnlyGuard.h"
#include <string>
#include <optional>
#include <fstream>
#include <cstdlib>
#include <poll.h>
#include <unistd.h>
#include <sys/wait.h>

namespace polaris::providers::real {

class RealKdeProvider {
public:
    static domain::DesktopInfo getDesktop() {
        domain::DesktopInfo d;
        const char* s = getenv("XDG_SESSION_TYPE"); if(s) d.sessionType=s; else d.sessionType="unknown";
        s = getenv("XDG_CURRENT_DESKTOP"); if(s) d.compositor = s;
        s = getenv("WAYLAND_DISPLAY"); if(s) d.sessionType="wayland";
        else {
            s=getenv("DISPLAY"); if(s) d.sessionType="x11";
        }
        // Plasmashell version via config? Try to read via safe exec if available
        // For P2, try fixed path /usr/bin/plasmashell --version via safeExec helper (read-only)
        // We'll attempt via helper class RealSystemdProvider::safeExec equivalent locally
        // To avoid duplication, just try to read version file if exists; fallback to "unknown"
        // Check kwinrc
        std::string home = getenv("HOME") ? getenv("HOME") : "/home/mehrangh";
        auto kw = safety::openReadOnly(home + "/.config/kwinrc");
        if(kw.is_open()){
            std::string line; bool inPlugins=false;
            while(std::getline(kw,line)){
                if(line=="[Plugins]") inPlugins=true;
                else if(!line.empty() && line[0]=='[') inPlugins=false;
                if(inPlugins){
                    if(line.find("blurEnabled=true")!=std::string::npos) d.effects["blur"]=true;
                    if(line.find("blurEnabled=false")!=std::string::npos) d.effects["blur"]=false;
                    if(line.find("glideEnabled=true")!=std::string::npos) d.effects["glide"]=true;
                }
            }
        }
        // Try to get plasmashell version via reading /usr/bin/plasmashell is binary, use safeExec if needed elsewhere
        return d;
    }

    static std::string getPlasmashellVersion() {
        // Use safe exec of fixed path
        // We inline minimal here to avoid dependency cycle
        std::string exe="/usr/bin/plasmashell";
        if(access(exe.c_str(), X_OK)!=0) return "";
        int pipefd[2]; if(pipe(pipefd)!=0) return "";
        pid_t pid=fork();
        if(pid==0){
            close(pipefd[0]); dup2(pipefd[1], STDOUT_FILENO); close(pipefd[1]);
            execl(exe.c_str(), "plasmashell","--version", (char*)nullptr);
            _exit(127);
        }
        close(pipefd[1]);
        std::string out; char buf[256]; ssize_t n;
        // simple blocking read with timeout via alarm? For P2 keep simple 2s via poll
        struct pollfd pfd{pipefd[0], POLLIN, 0};
        if(poll(&pfd,1,2000)>0) {
            n=read(pipefd[0],buf,sizeof(buf)-1);
            if(n>0){ buf[n]=0; out=buf; }
        }
        close(pipefd[0]);
        int status; waitpid(pid,&status,0);
        // out like "plasmashell 6.7.4\n"
        // trim
        if(!out.empty() && out.back()=='\n') out.pop_back();
        return out;
    }
};

} // namespace polaris::providers::real
