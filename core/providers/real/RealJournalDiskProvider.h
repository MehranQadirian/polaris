#pragma once
#include "../../domain/PerfModels.h"
#include "../../safety/ReadOnlyGuard.h"
#include <string>
#include <optional>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <regex>

namespace polaris::providers::real {

class RealJournalDiskProvider {
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

    static uint64_t parseSize(const std::string& s) {
        // Input like "3.2G", "850M", "500M", "1.5G", "1024K", "2048"
        std::string t=s;
        // trim
        t.erase(0,t.find_first_not_of(" \t\r\n"));
        t.erase(t.find_last_not_of(" \t\r\n")+1);
        if(t.empty()) return 0;
        // Find numeric part
        size_t pos=0;
        while(pos<t.size() && (isdigit(t[pos])||t[pos]=='.')) pos++;
        if(pos==0) return 0;
        std::string num=t.substr(0,pos);
        std::string unit=t.substr(pos);
        unit.erase(0,unit.find_first_not_of(" \t"));
        // Normalize unit
        for(auto &c: unit) c=tolower(c);
        double v=0;
        try{ v=std::stod(num); }catch(...){ return 0; }
        if(unit=="b"||unit=="bytes"||unit=="") return (uint64_t)v;
        if(unit=="k"||unit=="kb"||unit=="kib") return (uint64_t)(v*1024);
        if(unit=="m"||unit=="mb"||unit=="mib") return (uint64_t)(v*1024*1024);
        if(unit=="g"||unit=="gb"||unit=="gib") return (uint64_t)(v*1024*1024*1024);
        if(unit=="t"||unit=="tb") return (uint64_t)(v*1024ULL*1024*1024*1024);
        return (uint64_t)(v);
    }

    static domain::JournalDiskBaseline collect() {
        domain::JournalDiskBaseline j;
        j.meta = {"", "bytes", "journalctl --disk-usage", "exec fixed path", 0.85f, false, "unavailable: journalctl --disk-usage not collected"};
        j.vacuumTarget = "500M";
        auto out = safeExec("/usr/bin/journalctl", {"--disk-usage"}, 4);
        if(!out){
            out = safeExec("/bin/journalctl", {"--disk-usage"}, 4);
            if(!out){
                j.meta.available=false;
                j.meta.note="unavailable: journalctl not found or exec failed";
                return j;
            }
        }
        std::string &s=*out;
        // Example: "Archived and active journals take up 3.2G in the file system."
        // or "Journals take up 1.1G"
        uint64_t usage=0;
        // Find token that looks like size before "in the"
        // Use regex: (\d+\.?\d*\s*[KMGT]?B?) ? But we implement simple
        std::istringstream iss(s);
        std::string tok, prev;
        while(iss>>tok){
            // Check if tok is size and next is "in" or tok contains number and unit
            // Try parse tok as size
            uint64_t bytes = parseSize(tok);
            if(bytes>0){
                // Peek next token
                std::string next;
                auto pos = iss.tellg();
                if(iss>>next){
                    if(next=="in" || next=="B" || next=="G" || next=="M" || next=="K"){
                        // size token alone, unit may be separate
                        if(next=="in"){
                            // tok already had unit, like "3.2G"
                            usage = bytes;
                            break;
                        } else if(next=="B"||next=="M"||next=="G"||next=="K"){
                            // tok was number, next is unit
                            std::string combined = tok+next;
                            usage = parseSize(combined);
                            break;
                        }
                    }
                    // Not size, reset
                    iss.seekg(pos);
                } else {
                    // Last token was size
                    usage = bytes;
                    break;
                }
                // Also handle "3.2G" case where tok itself is size with unit and next is "in"
                if(bytes>0 && s.find(tok)!=std::string::npos){
                    // Check if tok contains G/M/K
                    if(tok.find('G')!=std::string::npos || tok.find('M')!=std::string::npos || tok.find('K')!=std::string::npos){
                        // Could be size, but continue to find larger?
                        if(usage==0) usage=bytes;
                    }
                }
            }
            prev=tok;
        }
        // Fallback: regex search for number+unit pattern
        if(usage==0){
            // Simple scan for "take up X"
            auto pos=s.find("take up");
            if(pos!=std::string::npos){
                std::string sub=s.substr(pos+7);
                std::istringstream subiss(sub);
                std::string a,b;
                if(subiss>>a){
                    // a might be "3.2G", try parse
                    usage = parseSize(a);
                    if(usage==0 && (subiss>>b)){
                        usage = parseSize(a+b);
                        if(usage==0) usage = parseSize(a+" "+b);
                    }
                }
            }
        }
        if(usage==0){
            j.meta.available=false;
            j.meta.note="unavailable: could not parse journalctl --disk-usage: " + s.substr(0,80);
            return j;
        }
        j.diskUsageBytes = usage;
        j.meta.available=true;
        j.meta.note="";
        // Determine vacuum target: fixed 500M for now
        j.vacuumTarget="500M";
        uint64_t targetBytes = parseSize(j.vacuumTarget);
        j.maxUsageBytes = targetBytes;
        if(usage > targetBytes) j.reclaimableBytes = usage - targetBytes;
        else j.reclaimableBytes = 0;
        j.meta.confidence=0.90f;
        return j;
    }

    static domain::JournalDiskBaseline fromFixture(const std::string& diskUsageOutput, const std::string& vacuumTarget="500M") {
        domain::JournalDiskBaseline j;
        j.vacuumTarget = vacuumTarget;
        j.meta = {"", "bytes", "fixture", "fixture", 0.90f, true, ""};
        // Parse same as collect but from string
        uint64_t usage = 0;
        // Try to find "take up"
        auto pos=diskUsageOutput.find("take up");
        std::string s=diskUsageOutput;
        if(pos!=std::string::npos){
            std::string sub=s.substr(pos+7);
            std::istringstream iss(sub);
            std::string a,b;
            if(iss>>a){
                usage = parseSize(a);
                if(usage==0 && (iss>>b)){
                    usage = parseSize(a+b);
                    if(usage==0) usage = parseSize(a+" "+b);
                }
            }
        }
        if(usage==0){
            // Try parse any token like "3.2G"
            std::istringstream iss(s);
            std::string tok;
            while(iss>>tok){
                uint64_t bytes = parseSize(tok);
                if(bytes> 1024*1024){ // heuristic >1M
                    usage = bytes;
                    break;
                }
            }
        }
        if(usage==0){
            j.meta.available=false;
            j.meta.note="unavailable: fixture parse failed";
            return j;
        }
        j.diskUsageBytes=usage;
        uint64_t targetBytes=parseSize(vacuumTarget);
        j.maxUsageBytes=targetBytes;
        if(usage>targetBytes) j.reclaimableBytes=usage-targetBytes;
        else j.reclaimableBytes=0;
        j.meta.available=true;
        return j;
    }
};

} // namespace polaris::providers::real
