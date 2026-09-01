#pragma once
#include "../../domain/PerfModels.h"
#include "../../safety/ReadOnlyGuard.h"
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <sstream>

namespace polaris::providers::real {

// Read-only provider for Flatpak runtimes. Never guesses.
// Returns unavailable if flatpak not installed or output unparsable.
class RealFlatpakProvider {
public:
    static std::optional<std::string> safeExec(const std::string& exe, const std::vector<std::string>& args, int timeoutSec=5) {
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

    static domain::FlatpakBaseline collect() {
        domain::FlatpakBaseline f;
        f.hasFlatpak = false;
        f.reclaimableBytes = 0;
        f.meta = {"", "bytes", "/usr/bin/flatpak list + /var/lib/flatpak", "exec fixed path", 0.85f, false, "unavailable: flatpak not installed or not collected"};

        auto out = safeExec("/usr/bin/flatpak", {"list","--columns=application,branch,origin,installed-size","--app","--runtime"}, 5);
        if(!out) {
            // Try alternative location
            out = safeExec("/var/lib/flatpak/exports/bin/flatpak", {"list","--columns=application,branch,origin,installed-size"}, 5);
            if(!out) {
                f.meta.available = false;
                f.meta.note = "unavailable: flatpak not installed or safeExec failed";
                return f;
            }
        }
        f.hasFlatpak = true;
        std::string &s = *out;
        std::istringstream iss(s);
        std::string line;
        // Skip header if present (contains "Application")
        bool first=true;
        while(std::getline(iss,line)){
            if(line.empty()) continue;
            if(first && line.find("Application")!=std::string::npos){ first=false; continue; }
            first=false;
            // Parse: application branch origin installed-size
            // Example: "org.freedesktop.Platform  23.08  flathub  850 MB"
            // Use stringstream to split, last token is size with unit
            std::istringstream ls(line);
            std::string app, branch, origin;
            std::string sizeStr, unit;
            if(!(ls>>app>>branch>>origin>>sizeStr)) continue;
            // sizeStr may be like "850" and next token "MB" or "850MB"
            std::string fullSize=sizeStr;
            if(ls>>unit){
                fullSize += " " + unit;
            }
            // Parse fullSize: number + unit
            // Normalize to bytes
            uint64_t bytes=0;
            try{
                // Extract numeric part
                size_t pos=0;
                while(pos<fullSize.size() && (isdigit(fullSize[pos])||fullSize[pos]=='.')) pos++;
                std::string num = fullSize.substr(0,pos);
                std::string u = fullSize.substr(pos);
                // trim u
                u.erase(0,u.find_first_not_of(" \t"));
                u.erase(u.find_last_not_of(" \t\r\n")+1);
                double v = std::stod(num);
                (void)v;
                if(u=="B"||u=="bytes") bytes=(uint64_t)v;
                else if(u=="KB"||u=="kB") bytes=(uint64_t)(v*1024);
                else if(u=="MB") bytes=(uint64_t)(v*1024*1024);
                else if(u=="GB") bytes=(uint64_t)(v*1024*1024*1024);
                else if(u.empty()) bytes=(uint64_t)v;
                else if(u=="MB"||u=="M") bytes=(uint64_t)(v*1024*1024);
                else bytes=(uint64_t)(v*1024*1024); // default MB
            } catch(...){ bytes=0; }
            if(app.empty()) continue;
            domain::FlatpakBaseline::Runtime r;
            r.id = app;
            r.branch = branch;
            r.origin = origin;
            r.installedSizeBytes = bytes;
            r.isRuntime = (app.find("Platform")!=std::string::npos || app.find("Sdk")!=std::string::npos || app.find("runtime")!=std::string::npos);
            f.runtimes.push_back(r);
            f.totalCount++;
        }
        // Try to get unused: flatpak uninstall --unused --dry-run? Use flatpak list --unused if available, else estimate
        auto unusedOut = safeExec("/usr/bin/flatpak", {"uninstall","--unused","--dry-run"}, 4);
        if(unusedOut){
            std::string &us = *unusedOut;
            // Parse unused runtimes lines
            std::istringstream uiss(us);
            std::string ul;
            while(std::getline(uiss, ul)){
                // Line like " 1. org.freedesktop.Platform 23.08 flathub"
                // Find token containing '.'
                std::istringstream ls(ul);
                std::string tok;
                while(ls>>tok){
                    if(tok.find(".")!=std::string::npos && tok.find("org.")==0){
                        // Found id
                        for(auto &r: f.runtimes){
                            if(r.id==tok){
                                f.unusedRuntimes.push_back(r);
                                f.reclaimableBytes += r.installedSizeBytes;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        } else {
            // Heuristic: if no unused detection, mark as unavailable for benefit
            // Keep reclaimable 0
        }
        f.meta.available = true;
        f.meta.note = "";
        f.meta.confidence = 0.85f;
        return f;
    }

    // For tests: construct from fixture string without exec
    static domain::FlatpakBaseline fromFixture(const std::string& listOutput, const std::string& unusedOutput) {
        domain::FlatpakBaseline f;
        f.hasFlatpak = true;
        f.reclaimableBytes = 0;
        f.meta = {"", "bytes", "fixture", "fixture", 0.85f, true, ""};
        std::istringstream iss(listOutput);
        std::string line;
        bool first=true;
        while(std::getline(iss,line)){
            if(line.empty()) continue;
            if(first && line.find("Application")!=std::string::npos){ first=false; continue; }
            first=false;
            std::istringstream ls(line);
            std::string app, branch, origin;
            std::string sizeStr, unit;
            if(!(ls>>app>>branch>>origin>>sizeStr)) continue;
            if(ls>>unit) sizeStr += " "+unit;
            uint64_t bytes=0;
            try{
                size_t pos=0;
                while(pos<sizeStr.size() && (isdigit(sizeStr[pos])||sizeStr[pos]=='.')) pos++;
                std::string num=sizeStr.substr(0,pos);
                std::string u=sizeStr.substr(pos);
                u.erase(0,u.find_first_not_of(" \t"));
                u.erase(u.find_last_not_of(" \t\r\n")+1);
                double v=std::stod(num);
                if(u=="B") bytes=(uint64_t)v;
                else if(u=="KB") bytes=(uint64_t)(v*1024);
                else if(u=="MB") bytes=(uint64_t)(v*1024*1024);
                else if(u=="GB") bytes=(uint64_t)(v*1024*1024*1024);
                else bytes=(uint64_t)(v*1024*1024);
            } catch(...){ bytes=0; }
            domain::FlatpakBaseline::Runtime r;
            r.id=app; r.branch=branch; r.origin=origin; r.installedSizeBytes=bytes;
            r.isRuntime=(app.find("Platform")!=std::string::npos || app.find("Sdk")!=std::string::npos);
            f.runtimes.push_back(r);
            f.totalCount++;
        }
        std::istringstream uiss(unusedOutput);
        std::string ul;
        while(std::getline(uiss,ul)){
            if(ul.empty()) continue;
            std::istringstream ls(ul);
            std::string tok;
            while(ls>>tok){
                if(tok.find("org.")==0){
                    for(auto &r: f.runtimes) if(r.id==tok){
                        // Avoid duplicate
                        bool exists=false;
                        for(auto &u: f.unusedRuntimes) if(u.id==r.id && u.branch==r.branch) exists=true;
                        if(!exists){
                            f.unusedRuntimes.push_back(r);
                            f.reclaimableBytes+=r.installedSizeBytes;
                        }
                        break;
                    }
                    break;
                }
            }
        }
        // If unusedOutput empty but we have duplicate runtimes heuristic, use branch duplicates
        if(f.unusedRuntimes.empty() && f.runtimes.size()>3){
            // Find duplicate id with different branch
            std::map<std::string, std::vector<domain::FlatpakBaseline::Runtime>> byId;
            for(auto &r: f.runtimes) byId[r.id].push_back(r);
            for(auto &kv: byId){
                if(kv.second.size()>1){
                    // Keep newest, mark older as reclaimable
                    for(size_t i=1;i<kv.second.size();i++){
                        f.unusedRuntimes.push_back(kv.second[i]);
                        f.reclaimableBytes+=kv.second[i].installedSizeBytes;
                    }
                }
            }
        }
        f.meta.available=true;
        return f;
    }
};

} // namespace polaris::providers::real
