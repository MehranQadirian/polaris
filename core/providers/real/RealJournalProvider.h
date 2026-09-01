#pragma once
#include "../../safety/ReadOnlyGuard.h"
#include <string>
#include <vector>
#include <optional>

namespace polaris::providers::real {

class RealJournalProvider {
public:
    static std::optional<std::string> safeExecGet(const std::string& exe, const std::vector<std::string>& args, int timeoutSec=4);

    static std::vector<std::string> getPriorityErrors(int priority=3, int lines=50) {
        std::vector<std::string> out;
        auto o = safeExecGet("/usr/bin/journalctl", {"--no-pager","-p",std::to_string(priority),"-b","-n",std::to_string(lines)}, 4);
        if(!o) return out;
        std::string& s=*o;
        size_t pos=0;
        while(pos < s.size()){
            auto nl=s.find('\n',pos);
            std::string line=s.substr(pos, nl==std::string::npos?std::string::npos:nl-pos);
            if(!line.empty()) out.push_back(line);
            if(nl==std::string::npos) break;
            pos=nl+1;
        }
        return out;
    }

    static std::vector<std::string> getNvidiaErrors() {
        std::vector<std::string> out;
        auto o = safeExecGet("/usr/bin/journalctl", {"--no-pager","-b","--grep=nvidia|NVRM","-n","50"}, 4);
        if(!o){
            auto o2 = safeExecGet("/usr/bin/journalctl", {"--no-pager","-b","-n","200"}, 4);
            if(!o2) return out;
            std::string& s=*o2;
            size_t pos=0;
            while(pos < s.size()){
                auto nl=s.find('\n',pos);
                std::string line=s.substr(pos, nl==std::string::npos?std::string::npos:nl-pos);
                if(line.find("nvidia")!=std::string::npos || line.find("NVRM")!=std::string::npos) out.push_back(line);
                if(nl==std::string::npos) break;
                pos=nl+1;
                if(out.size()>=50) break;
            }
            return out;
        }
        std::string& s=*o;
        size_t pos=0;
        while(pos < s.size()){
            auto nl=s.find('\n',pos);
            std::string line=s.substr(pos, nl==std::string::npos?std::string::npos:nl-pos);
            out.push_back(line);
            if(nl==std::string::npos) break;
            pos=nl+1;
        }
        return out;
    }

    static int countPriority(int priority=3) {
        // Use --no-pager -p 3 -b --lines=? to limit, count via wc logic
        // Use longer timeout and limit lines to avoid huge output
        auto o = safeExecGet("/usr/bin/journalctl", {"--no-pager","-p",std::to_string(priority),"-b","-n","500"}, 6);
        if(!o) return -1;
        int cnt=0;
        std::string& s=*o;
        size_t pos=0;
        while((pos=s.find('\n',pos))!=std::string::npos){ cnt++; pos++; }
        // If we hit 500 lines, indicate truncated but still count
        return cnt;
    }
};

} // namespace polaris::providers::real
