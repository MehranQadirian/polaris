#pragma once
#include "../../domain/Perf.h"
#include "../../safety/ReadOnlyGuard.h"
#include <string>
#include <vector>
#include <dirent.h>
#include <fstream>
#include <unistd.h>

namespace polaris::providers::real {

class RealProcessProvider {
public:
    static std::vector<domain::ProcessInfo> getTop(int limit=20, bool sortByCpu=true) {
        (void)sortByCpu;
        std::vector<domain::ProcessInfo> out;
        DIR* d = opendir("/proc");
        if(!d) return out;
        struct dirent* e;
        while((e=readdir(d))){
            if(!isdigit(e->d_name[0])) continue;
            int pid = atoi(e->d_name);
            std::string base = std::string("/proc/") + e->d_name + "/";
            // cmdline
            std::string name;
            auto f = safety::openReadOnly(base + "comm");
            if(f.is_open()) std::getline(f, name);
            if(name.empty()){
                auto ff = safety::openReadOnly(base + "cmdline");
                if(ff.is_open()){
                    std::string cmd((std::istreambuf_iterator<char>(ff)), std::istreambuf_iterator<char>());
                    // cmdline null separated, take first
                    size_t nul = cmd.find('\0');
                    if(nul!=std::string::npos) cmd=cmd.substr(0,nul);
                    name=cmd;
                }
            }
            // status for mem
            std::string line;
            uint64_t rss=0;
            auto sf = safety::openReadOnly(base + "status");
            if(sf.is_open()){
                while(std::getline(sf,line)){
                    if(line.rfind("VmRSS:",0)==0){
                        unsigned long v=0; sscanf(line.c_str(),"VmRSS: %lu",&v);
                        rss=v;
                    }
                }
            }
            // stat for cpu (utime, stime) - for P2 we just collect rss, cpu via /proc/stat? Keep simple: mem sort
            domain::ProcessInfo p;
            p.pid=pid; p.name=name; p.rssKb=rss;
            p.mem = (float)rss / 100.0f;
            if (name.empty()) continue; // skip kernel threads without name for top
            out.push_back(p);
        }
        closedir(d);
        std::sort(out.begin(), out.end(), [](auto& a, auto& b){return a.rssKb > b.rssKb;});
        if((int)out.size()>limit) out.resize(limit);
        return out;
    }

    static std::string getLoadAvg() {
        auto f = safety::openReadOnly("/proc/loadavg");
        if(!f.is_open()) return "";
        std::string line; std::getline(f,line);
        return line;
    }
};

} // namespace polaris::providers::real
