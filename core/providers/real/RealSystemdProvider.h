#pragma once
#include "../../domain/Perf.h"
#include "../../safety/ReadOnlyGuard.h"
#include <string>
#include <vector>
#include <optional>
#include <sys/wait.h>
#include <unistd.h>

namespace polaris::providers::real {

class RealSystemdProvider {
public:
    // Read-only via safe exec of fixed binaries with no shell, separate args, timeout.
    // Prefer D-Bus eventually, but for P2 this is safe read-only exec.
    static std::optional<std::string> safeExec(const std::string& exe, const std::vector<std::string>& args, int timeoutSec=5);

    static std::vector<domain::ServiceInfo> getFailedServices() {
        std::vector<domain::ServiceInfo> out;
        auto o = safeExec("/usr/bin/systemctl", {"--failed","--no-pager","--no-legend"}, 5);
        if(!o) return out;
        std::string& s=*o;
        size_t pos=0;
        while(pos < s.size()){
            auto nl=s.find('\n',pos);
            std::string line = s.substr(pos, nl==std::string::npos?std::string::npos:nl-pos);
            // systemctl --failed with --no-legend may have bullet "●"
            // Find first alphanumeric token containing ".service"
            size_t svc = line.find(".service");
            if(svc != std::string::npos){
                // find start of token
                size_t start = line.rfind(' ', svc);
                if(start==std::string::npos) start=0; else start++;
                // also handle bullet "● "
                if(line[start]=='\0' || line[start]==' ') start++;
                // find token bounds
                size_t end = svc + 8; // ".service" len 8
                domain::ServiceInfo si;
                si.name = line.substr(start, end - start);
                // trim bullet and spaces
                while(!si.name.empty() && (si.name[0]==' ' || si.name[0]=='\xE2' || si.name[0]=='\x97' || si.name[0]=='\x97' || si.name[0]=='*')) si.name.erase(0,1);
                // better: extract via sscanf
                char name[256]={0};
                if(sscanf(line.c_str(),"%*s %255s", name)==1) {
                    // sscanf with bullet: first token is "●", second is name
                    if(std::string(name).find(".service")!=std::string::npos) si.name=name;
                }
                // fallback: if line starts with bullet, second token is name
                if(si.name.find(".service")==std::string::npos){
                    char bullet[16], n2[256];
                    if(sscanf(line.c_str(),"%15s %255s", bullet, n2)==2 && std::string(n2).find(".service")!=std::string::npos) si.name=n2;
                }
                if(si.name.find(".service")!=std::string::npos){
                    si.failed=true;
                    out.push_back(si);
                }
            }
            if(nl==std::string::npos) break;
            pos=nl+1;
        }
        return out;
    }

    static domain::BootInfo getBoot() {
        domain::BootInfo b;
        auto o = safeExec("/usr/bin/systemd-analyze", {}, 5);
        if(o){
            std::string& s=*o;
            float fw=0, lo=0, ke=0, init=0, us=0;
            // Robust parse: extract numbers before 's (firmware)' etc via search
            auto extract = [&](const std::string& marker, float& out){
                auto p = s.find(marker);
                if(p==std::string::npos) return;
                // find number start backwards
                size_t start = p;
                while(start>0 && (isdigit(s[start-1]) || s[start-1]=='.')) start--;
                std::string num = s.substr(start, p - start);
                try{ out = std::stof(num); }catch(...){}
            };
            // Try sscanf first, fallback to extract
            int n = sscanf(s.c_str(),"Startup finished in %fs (firmware) + %fs (loader) + %fs (kernel) + %fs (initrd) + %fs (userspace)",&fw,&lo,&ke,&init,&us);
            if(n<5){
                extract("s (firmware)", fw);
                extract("s (loader)", lo);
                extract("s (kernel)", ke);
                extract("s (initrd)", init);
                extract("s (userspace)", us);
            }
            b.firmware=fw; b.loader=lo; b.kernel=ke; b.initrd=init; b.userspace=us;
        }
        auto blame = safeExec("/usr/bin/systemd-analyze", {"blame","--no-pager"}, 10);
        if(blame){
            std::string& s=*blame;
            size_t pos=0; int cnt=0;
            while(pos < s.size() && cnt<20){
                auto nl=s.find('\n',pos);
                std::string line=s.substr(pos, nl==std::string::npos?std::string::npos:nl-pos);
                // line: "45.052s packagekit.service"
                float sec=0; char name[256];
                if(sscanf(line.c_str(),"%fs %255s",&sec,name)==2){
                    b.blameTop.emplace_back(name, sec);
                }
                if(nl==std::string::npos) break;
                pos=nl+1; cnt++;
            }
        }
        auto cc = safeExec("/usr/bin/systemd-analyze", {"critical-chain","--no-pager"}, 5);
        if(cc) b.criticalChain=*cc;
        return b;
    }

    static std::string getCriticalChain(){
        auto o = safeExec("/usr/bin/systemd-analyze", {"critical-chain","--no-pager"},5);
        return o?*o:"";
    }
};

} // namespace polaris::providers::real
