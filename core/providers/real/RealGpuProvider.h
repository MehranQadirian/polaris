#pragma once
#include "../../domain/HardwareInfo.h"
#include "../../safety/ReadOnlyGuard.h"
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <optional>
#include <vector>

namespace polaris::providers::real {

class RealGpuProvider {
public:
    static std::vector<domain::GpuInfo> getGpus() {
        std::vector<domain::GpuInfo> out;
        // Walk /sys/bus/pci/devices
        DIR* d = opendir("/sys/bus/pci/devices");
        if(!d) return out;
        struct dirent* e;
        while((e=readdir(d))){
            if(e->d_name[0]=='.') continue;
            std::string pci = e->d_name; // 0000:00:02.0
            std::string base = std::string("/sys/bus/pci/devices/") + pci + "/";
            auto readFile = [&](const std::string& name)->std::string{
                auto f = safety::openReadOnly(base + name);
                if(!f.is_open()) return "";
                std::string v; std::getline(f, v);
                // trim
                if(!v.empty() && v.back()=='\n') v.pop_back();
                return v;
            };
            std::string cls = readFile("class");
            // class 0x030000 VGA, 0x030200 3D controller, 0x038000 Display
            if(cls.empty()) continue;
            // cls like 0x030000 -> check prefix 0x03
            if(cls.rfind("0x03",0)!=0) continue;
            domain::GpuInfo gpu;
            gpu.pciId = pci;
            std::string vendor = readFile("vendor"); // 0x8086
            std::string device = readFile("device"); // 0x9b41
            gpu.vendor = vendor; gpu.model = device; // will resolve via modalias? keep raw
            // resolve human name via /sys/bus/pci/devices/.../uevent or modalias? For P2 keep vendor:device
            // driver symlink
            char link[512]; ssize_t len = readlink((base+"driver").c_str(), link, sizeof(link)-1);
            if(len>0){ link[len]=0; std::string drv(link); auto slash=drv.rfind('/'); gpu.driver = (slash==std::string::npos?drv:drv.substr(slash+1)); gpu.module=gpu.driver; gpu.claimed=true; }
            else { gpu.claimed=false; }
            // For claimed, try to get glRenderer via sysfs? Not; will be filled via helper
            out.push_back(gpu);
        }
        closedir(d);
        // Augment vendor/device human names via /usr/share/hwdata/pci.ids read-only parse if available
        for(auto& g: out){
            // try to resolve vendor/device via pci.ids
            auto f = safety::openReadOnly("/usr/share/hwdata/pci.ids");
            if(!f.is_open()) f = safety::openReadOnly("/usr/share/pci.ids");
            if(f.is_open()){
                std::string line; std::string curVendor;
                std::string wantV = g.vendor; // 0x8086 -> 8086
                if(wantV.rfind("0x",0)==0) wantV=wantV.substr(2);
                std::string wantD = g.model; if(wantD.rfind("0x",0)==0) wantD=wantD.substr(2);
                // pci.ids format: vendor <id>  <name>
                // \t device <id>  <name>
                while(std::getline(f,line)){
                    if(line.empty() || line[0]=='#') continue;
                    if(line[0]!='\t' && line[0]!=' '){
                        // vendor line
                        if(line.size()>=4 && line.substr(0,4)==wantV) curVendor=line.substr(5);
                    } else if(!curVendor.empty() && line.size()>1 && line[0]=='\t'){
                        std::string devId = line.substr(1,4);
                        if(devId==wantD){
                            g.model = curVendor + " " + line.substr(6);
                            break;
                        }
                    }
                }
            }
            if(g.vendor=="0x8086") g.vendor="Intel";
            else if(g.vendor=="0x10de") g.vendor="NVIDIA";
        }
        return out;
    }

    struct NvidiaState {
        bool moduleLoaded=false;
        std::string version; // from /sys/module/nvidia/version or /proc/driver/nvidia/version
        bool gspFirmware=false; // from modinfo firmware presence (read-only)
        bool nvidiaSmiAvailable=false;
        std::string nvidiaSmiError;
        bool prime=false;
    };

    static NvidiaState getNvidiaState() {
        NvidiaState s;
        // /sys/module/nvidia/version
        auto f = safety::openReadOnly("/sys/module/nvidia/version");
        if(f.is_open()){ std::getline(f, s.version); s.moduleLoaded=true; }
        // /proc/driver/nvidia/version
        auto f2 = safety::openReadOnly("/proc/driver/nvidia/version");
        if(f2.is_open()){ std::string l; std::getline(f2,l); if(!l.empty()) s.version=l; s.moduleLoaded=true; }
        // check /proc/modules for nvidia
        auto f3 = safety::openReadOnly("/proc/modules");
        if(f3.is_open()){
            std::string line; while(std::getline(f3,line)) if(line.rfind("nvidia",0)==0) { s.moduleLoaded=true; break; }
        }
        // nvidia-smi existence via fixed path check (no exec yet)
        struct stat st;
        s.nvidiaSmiAvailable = (stat("/usr/bin/nvidia-smi",&st)==0);
        // GSP: check firmware files existence
        s.gspFirmware = (access("/lib/firmware/nvidia", F_OK)==0);
        // PRIME: check /sys/kernel/debug/vgaswitcheroo or prime offload via /usr/share/X11
        if(access("/sys/kernel/debug/vgaswitcheroo/switch", F_OK)==0) s.prime=true;
        // Also check /usr/lib64/nvidia/xorg exists
        if(access("/usr/lib64/nvidia", F_OK)==0) s.prime=true;
        return s;
    }

    // Safe exec helper for read-only utilities with fixed paths, no shell, timeout, no injection.
    // For OpenGL/Vulkan we attempt to run /usr/bin/glxinfo and /usr/bin/vulkaninfo if present, capturing output.
    static std::optional<std::string> safeExecGetOutput(const std::string& exe, const std::vector<std::string>& args, int timeoutSec=2);

    static std::string getGlRenderer() {
        std::string disp = getenv("DISPLAY") ? getenv("DISPLAY") : "";
        bool needTemp = disp.empty();
        if(needTemp) setenv("DISPLAY", ":0", 1);
        auto out = safeExecGetOutput("/usr/bin/glxinfo", {"-B"}, 4);
        if(needTemp) {
            // restore
            if(disp.empty()) unsetenv("DISPLAY");
            else setenv("DISPLAY", disp.c_str(), 1);
        }
        if(!out || out->empty()){
            return "Permission required - skipped in P2 or headless (no DISPLAY)";
        }
        std::string& s=*out;
        auto pos=s.find("OpenGL renderer string:");
        if(pos==std::string::npos) pos=s.find("renderer string:");
        if(pos!=std::string::npos){
            auto e=s.find('\n',pos);
            return s.substr(pos, e==std::string::npos?std::string::npos:e-pos);
        }
        auto intel = s.find("Intel");
        if(intel!=std::string::npos){ auto e=s.find('\n',intel); return s.substr(intel, std::min<size_t>(80, e-intel)); }
        return s.substr(0,200);
    }

    static std::string getVulkanInfo() {
        auto out = safeExecGetOutput("/usr/bin/vulkaninfo", {"--summary"}, 4);
        if(!out || out->empty()) return "Permission required - skipped in P2 or no Vulkan ICD";
        // Contains warnings about dzn driver, filter
        auto pos = out->find("Vulkan Instance Version");
        if(pos!=std::string::npos) return out->substr(pos, 300);
        return out->substr(0,500);
    }
};

} // namespace polaris::providers::real
