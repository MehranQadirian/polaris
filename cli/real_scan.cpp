#include "../core/providers/real/RealOsProvider.h"
#include "../core/providers/real/RealCpuProvider.h"
#include "../core/providers/real/RealMemoryProvider.h"
#include "../core/providers/real/RealStorageProvider.h"
#include "../core/providers/real/RealThermalProvider.h"
#include "../core/providers/real/RealGpuProvider.h"
#include "../core/providers/real/RealSystemdProvider.h"
#include "../core/providers/real/RealKdeProvider.h"
#include "../core/providers/real/RealProcessProvider.h"
#include "../core/providers/real/RealJournalProvider.h"
#include "../core/safety/ReadOnlyGuard.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <unistd.h>

std::string json_escape(const std::string& s){
    std::string out;
    for(char c: s){
        if(c=='"') out += "\\\"";
        else if(c=='\\') out += "\\\\";
        else if(c=='\n') out += "\\n";
        else if(c=='\r') out += "\\r";
        else if(c=='\t') out += "\\t";
        else if((unsigned char)c<0x20){
            char buf[7]; snprintf(buf,sizeof(buf),"\\u%04x",(unsigned)c); out+=buf;
        } else out+=c;
    }
    return out;
}

int main(int argc, char** argv){
    using namespace polaris::providers::real;
    bool jsonMode=false;
    bool human=false;
    for(int i=1;i<argc;i++){
        std::string a=argv[i];
        if(a=="--json") jsonMode=true;
        if(a=="--human") human=true;
    }
    if(!jsonMode && !human) jsonMode=true; // default json

    auto start = std::chrono::steady_clock::now();

    // Enforce read-only
    polaris::safety::enforceReadOnly("real_scan start");

    // Collect
    auto os = RealOsProvider::getOs();
    auto kernel = RealOsProvider::getKernel();
    auto desktop = RealKdeProvider::getDesktop();
    std::string plasmaVer = RealKdeProvider::getPlasmashellVersion();
    auto cpu = RealCpuProvider::getCpu();
    auto mem = RealMemoryProvider::get();
    auto fs = RealStorageProvider::getFilesystems();
    auto blks = RealStorageProvider::getBlockDevices();
    auto therm = RealThermalProvider::getThermals();
    auto gpus = RealGpuProvider::getGpus();
    auto nvidia = RealGpuProvider::getNvidiaState();
    std::string glRenderer = RealGpuProvider::getGlRenderer();
    std::string vulkan = RealGpuProvider::getVulkanInfo();
    auto boot = RealSystemdProvider::getBoot();
    auto failed = RealSystemdProvider::getFailedServices();
    auto processes = RealProcessProvider::getTop(15);
    std::string loadavg = RealProcessProvider::getLoadAvg();
    auto journalP3 = RealJournalProvider::getPriorityErrors(3, 20);
    auto nvidiaErrs = RealJournalProvider::getNvidiaErrors();
    int p3count = RealJournalProvider::countPriority(3);
    std::string sessionType = getenv("XDG_SESSION_TYPE")?getenv("XDG_SESSION_TYPE"):"unknown";
    std::string wayland = getenv("WAYLAND_DISPLAY")?getenv("WAYLAND_DISPLAY"):"";
    std::string display = getenv("DISPLAY")?getenv("DISPLAY"):"";

    auto end = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(end-start).count();

    // Human readable if requested
    if(human){
        std::cout << "# Polaris P2 Real Read-Only Scan - " << os.prettyName << " - no writes, no sudo\n";
        std::cout << "OS: " << os.prettyName << " variant=" << os.variantId << " arch=" << os.arch << "\n";
        std::cout << "Kernel: " << kernel.version << "\n";
        std::cout << "Cmdline: " << kernel.cmdline.substr(0,120) << "\n";
        std::cout << "Desktop: " << plasmaVer << " session=" << sessionType << " wayland=" << wayland << " display=" << display << "\n";
        std::cout << "CPU: " << cpu.model << " cores=" << cpu.cores << " threads=" << cpu.threads << " gov=" << cpu.governor << " epp=" << cpu.epp << " cur=" << cpu.curMhz << "MHz\n";
        std::cout << "Memory: totalKb=" << mem.totalKb << " avail=" << mem.availableKb << " swap=" << mem.swap.total << "/" << mem.swap.used << " zram=" << mem.zram.disksize << "\n";
        std::cout << "Filesystems: " << fs.size() << "\n";
        for(auto &f: fs) std::cout << "  " << f.device << " -> " << f.mount << " " << f.fstype << " " << f.usedBytes/1024/1024 << "M used\n";
        std::cout << "Block: " << blks.size() << "\n";
        for(auto &b: blks) std::cout << "  " << b.name << " " << b.model << " " << b.type << " " << b.sizeBytes/1024/1024/1024 << "GB\n";
        std::cout << "Thermals: " << therm.size() << "\n";
        for(auto &t: therm) std::cout << "  " << t.label << " " << t.source << " " << t.tempC << "C\n";
        std::cout << "GPUs: " << gpus.size() << "\n";
        for(auto &g: gpus) std::cout << "  " << g.vendor << " " << g.model << " driver=" << g.driver << " claimed=" << g.claimed << "\n";
        std::cout << "Nvidia moduleLoaded=" << nvidia.moduleLoaded << " version=" << nvidia.version << " smiAvail=" << nvidia.nvidiaSmiAvailable << "\n";
        std::cout << "GL: " << glRenderer.substr(0,120) << "\n";
        std::cout << "Vulkan: " << vulkan.substr(0,120) << "\n";
        std::cout << "Boot: firmware=" << boot.firmware << " loader=" << boot.loader << " kernel=" << boot.kernel << " initrd=" << boot.initrd << " userspace=" << boot.userspace << "\n";
        std::cout << "Blame top:\n"; for(auto &p: boot.blameTop) std::cout << "  " << p.first << " " << p.second << "s\n";
        std::cout << "Failed: " << failed.size() << "\n"; for(auto &f: failed) std::cout << "  " << f.name << "\n";
        std::cout << "Loadavg: " << loadavg << "\n";
        std::cout << "Processes top:\n"; for(auto &pr: processes) std::cout << "  " << pr.pid << " " << pr.name << " rss=" << pr.rssKb << "\n";
        std::cout << "Journal p3 count=" << p3count << " recent=" << journalP3.size() << "\n";
        std::cout << "Nvidia errs: " << nvidiaErrs.size() << "\n";
        std::cout << "Scan overhead: " << elapsedMs << "ms - read-only, no sudo, no writes\n";
        return 0;
    }

    // JSON
    std::ostringstream j;
    j << "{\n";
    j << "  \"meta\": {\"mode\":\"P2_READ_ONLY\",\"elapsedMs\":" << elapsedMs << ",\"readOnlyGuard\":true,\"noSudo\":true},\n";
    j << "  \"system\": {\"os\":{\"pretty\":\"" << json_escape(os.prettyName) << "\",\"id\":\"" << json_escape(os.distro) << "\",\"variant\":\"" << json_escape(os.variantId) << "\",\"version\":\"" << json_escape(os.versionId) << "\",\"arch\":\"" << json_escape(os.arch) << "\"},";
    j << "\"kernel\":{\"version\":\"" << json_escape(kernel.version) << "\",\"cmdline\":\"" << json_escape(kernel.cmdline) << "\"},";
    j << "\"desktop\":{\"plasmashell\":\"" << json_escape(plasmaVer) << "\",\"sessionType\":\"" << json_escape(sessionType) << "\",\"wayland\":\"" << json_escape(wayland) << "\",\"display\":\"" << json_escape(display) << "\"}},\n";
    j << "  \"cpu\": {\"model\":\"" << json_escape(cpu.model) << "\",\"cores\":" << cpu.cores << ",\"threads\":" << cpu.threads << ",\"driver\":\"" << json_escape(cpu.scalingDriver) << "\",\"governor\":\"" << json_escape(cpu.governor) << "\",\"epp\":\"" << json_escape(cpu.epp) << "\",\"curMhz\":" << cpu.curMhz << ",\"minMhz\":" << cpu.freqMinMhz << ",\"maxMhz\":" << cpu.freqMaxMhz << ",\"noTurbo\":" << (cpu.noTurbo?"true":"false") << "},\n";
    j << "  \"memory\": {\"totalKb\":" << mem.totalKb << ",\"availableKb\":" << mem.availableKb << ",\"cachedKb\":" << mem.cachedKb << ",\"swappiness\":" << mem.swappiness << ",\"vfsPressure\":" << mem.vfsCachePressure << ",\"pressure\":{\"someAvg10\":" << mem.pressure.someAvg10 << "},\"swap\":{\"total\":" << mem.swap.total << ",\"used\":" << mem.swap.used << "},\"zram\":{\"algo\":\"" << json_escape(mem.zram.algo) << "\",\"disksize\":" << mem.zram.disksize << ",\"data\":" << mem.zram.data << "}},\n";
    j << "  \"filesystems\": [";
    for(size_t i=0;i<fs.size();++i){ auto &f=fs[i]; j << "{\"device\":\"" << json_escape(f.device) << "\",\"mount\":\"" << json_escape(f.mount) << "\",\"fstype\":\"" << json_escape(f.fstype) << "\",\"size\":" << f.sizeBytes << ",\"free\":" << f.freeBytes << "}"; if(i+1<fs.size()) j<<","; }
    j << "],\n";
    j << "  \"blockDevices\": [";
    for(size_t i=0;i<blks.size();++i){ auto &b=blks[i]; j << "{\"name\":\"" << json_escape(b.name) << "\",\"model\":\"" << json_escape(b.model) << "\",\"type\":\"" << json_escape(b.type) << "\",\"size\":" << b.sizeBytes << "}"; if(i+1<blks.size()) j<<","; }
    j << "],\n";
    j << "  \"thermals\": [";
    for(size_t i=0;i<therm.size();++i){ auto &t=therm[i]; j << "{\"source\":\"" << json_escape(t.source) << "\",\"label\":\"" << json_escape(t.label) << "\",\"tempC\":" << t.tempC << "}"; if(i+1<therm.size()) j<<","; }
    j << "],\n";
    j << "  \"gpus\": [";
    for(size_t i=0;i<gpus.size();++i){ auto &g=gpus[i]; j << "{\"vendor\":\"" << json_escape(g.vendor) << "\",\"model\":\"" << json_escape(g.model) << "\",\"pci\":\"" << json_escape(g.pciId) << "\",\"driver\":\"" << json_escape(g.driver) << "\",\"claimed\":" << (g.claimed?"true":"false") << "}"; if(i+1<gpus.size()) j<<","; }
    j << "],\n";
    j << "  \"nvidia\": {\"moduleLoaded\":" << (nvidia.moduleLoaded?"true":"false") << ",\"version\":\"" << json_escape(nvidia.version) << "\",\"smiAvailable\":" << (nvidia.nvidiaSmiAvailable?"true":"false") << "},\n";
    j << "  \"glRenderer\": \"" << json_escape(glRenderer) << "\",\n";
    j << "  \"vulkan\": \"" << json_escape(vulkan.substr(0,300)) << "\",\n";
    j << "  \"boot\": {\"firmware\":" << boot.firmware << ",\"loader\":" << boot.loader << ",\"kernel\":" << boot.kernel << ",\"initrd\":" << boot.initrd << ",\"userspace\":" << boot.userspace << ",\"blameTop\":[";
    for(size_t i=0;i<boot.blameTop.size();++i){ j << "{\"unit\":\"" << json_escape(boot.blameTop[i].first) << "\",\"sec\":" << boot.blameTop[i].second << "}"; if(i+1<boot.blameTop.size()) j<<","; }
    j << "]},\n";
    j << "  \"failedServices\": [";
    for(size_t i=0;i<failed.size();++i){ j << "\"" << json_escape(failed[i].name) << "\""; if(i+1<failed.size()) j<<","; }
    j << "],\n";
    j << "  \"processes\": [";
    for(size_t i=0;i<processes.size();++i){ auto &p=processes[i]; j << "{\"pid\":" << p.pid << ",\"name\":\"" << json_escape(p.name) << "\",\"rssKb\":" << p.rssKb << "}"; if(i+1<processes.size()) j<<","; }
    j << "],\n";
    j << "  \"loadavg\": \"" << json_escape(loadavg) << "\",\n";
    j << "  \"journal\": {\"p3count\":" << p3count << ",\"p3sample\":[";
    for(size_t i=0;i<journalP3.size() && i<10;++i){ j << "\"" << json_escape(journalP3[i].substr(0,120)) << "\""; if(i+1<journalP3.size() && i+1<10) j<<","; }
    j << "],\"nvidiaErrs\":" << nvidiaErrs.size() << "}\n";
    j << "}\n";
    std::cout << j.str();
    return 0;
}
