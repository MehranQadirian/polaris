#pragma once
#include "../../domain/SystemInfo.h"
#include "../../safety/ReadOnlyGuard.h"
#include <string>
#include <map>
#include <fstream>
#include <sys/utsname.h>

namespace polaris::providers::real {

class RealOsProvider {
public:
    // Read-only: parses /etc/os-release and /etc/fedora-release, no writes, no sudo.
    static domain::OsInfo getOs() {
        domain::OsInfo os;
        auto f = safety::openReadOnly("/etc/os-release");
        if (f.is_open()) {
            std::string line;
            std::map<std::string,std::string> kv;
            while (std::getline(f, line)) {
                auto eq = line.find('=');
                if (eq==std::string::npos) continue;
                std::string k=line.substr(0,eq);
                std::string v=line.substr(eq+1);
                if (!v.empty() && v.front()=='"') { v=v.substr(1); if (!v.empty() && v.back()=='"') v.pop_back(); }
                kv[k]=v;
            }
            os.distro = kv.count("ID") ? kv["ID"] : "fedora";
            os.variantId = kv.count("VARIANT_ID") ? kv["VARIANT_ID"] : "";
            os.versionId = kv.count("VERSION_ID") ? kv["VERSION_ID"] : "";
            os.prettyName = kv.count("PRETTY_NAME") ? kv["PRETTY_NAME"] : "";
            os.arch = "x86_64"; // from uname below
        }
        struct utsname u;
        if (uname(&u)==0) os.arch = u.machine;
        return os;
    }
    static domain::KernelInfo getKernel() {
        domain::KernelInfo k;
        struct utsname u;
        if (uname(&u)==0) { k.version = std::string(u.release) + " " + u.machine; }
        auto f = safety::openReadOnly("/proc/cmdline");
        if (f.is_open()) { std::getline(f, k.cmdline); }
        return k;
    }
    static domain::DesktopInfo getDesktop() {
        domain::DesktopInfo d;
        const char* s = getenv("XDG_SESSION_TYPE"); if(s) d.sessionType=s;
        s = getenv("XDG_CURRENT_DESKTOP"); if(s) {
            // KDE check
            std::string v=s; if(v.find("KDE")!=std::string::npos) d.plasma="KDE";
        }
        s = getenv("WAYLAND_DISPLAY"); if(s) { /* wayland */ }
        // Read plasmashell version via fixed path exec fallback - but in provider we just read via safe file check
        // For P2 we read kwinrc to infer effects (read-only)
        auto kw = safety::openReadOnly(std::string(getenv("HOME")?getenv("HOME"):"/home/mehrangh") + "/.config/kwinrc");
        if (kw.is_open()) {
            std::string line; bool inPlugins=false;
            while (std::getline(kw,line)) {
                if(line.find("[Plugins]")!=std::string::npos) inPlugins=true;
                else if(line.rfind("[",0)==0) inPlugins=false;
                if(inPlugins) {
                    if(line.find("blurEnabled=true")!=std::string::npos) d.effects["blur"]=true;
                    if(line.find("blurEnabled=false")!=std::string::npos) d.effects["blur"]=false;
                    if(line.find("glideEnabled=true")!=std::string::npos) d.effects["glide"]=true;
                    if(line.find("glideEnabled=false")!=std::string::npos) d.effects["glide"]=false;
                }
                if(line.find("plasmashell")!=std::string::npos) {} // ignore
            }
        }
        // Try to get plasmashell version via fixed path exec (read-only, timeout)
        // Fallback to parsing via real provider helper (see RealKdeProvider)
        return d;
    }
};

} // namespace polaris::providers::real
