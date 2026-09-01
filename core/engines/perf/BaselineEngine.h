#pragma once
#include "../../domain/PerfModels.h"
#include "../../providers/real/RealOsProvider.h"
#include "../../providers/real/RealCpuProvider.h"
#include "../../providers/real/RealMemoryProvider.h"
#include "../../providers/real/RealStorageProvider.h"
#include "../../providers/real/RealThermalProvider.h"
#include "../../providers/real/RealGpuProvider.h"
#include "../../providers/real/RealSystemdProvider.h"
#include "../../providers/real/RealKdeProvider.h"
#include "../../providers/real/RealProcessProvider.h"
#include "../../providers/real/RealJournalProvider.h"
#include <chrono>
#include <ctime>

namespace polaris::engines::perf {

class BaselineEngine {
public:
    static domain::PerformanceBaseline collect() {
        domain::PerformanceBaseline b;
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        char buf[64]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S%z", localtime(&tt));
        b.timestamp = buf;
        b.id = buf;

        // CPU
        {
            auto cpu = providers::real::RealCpuProvider::getCpu();
            b.cpu.model = cpu.model;
            b.cpu.cores = cpu.cores; b.cpu.threads = cpu.threads;
            b.cpu.driver = cpu.scalingDriver; b.cpu.governor = cpu.governor; b.cpu.epp = cpu.epp;
            b.cpu.curMhz = cpu.curMhz; b.cpu.minMhz = cpu.freqMinMhz; b.cpu.maxMhz = cpu.freqMaxMhz;
            b.cpu.noTurbo = cpu.noTurbo;
            auto f = safety::openReadOnly("/proc/loadavg");
            if(f.is_open()) std::getline(f, b.cpu.loadAvg);
            {
                auto ff = safety::openReadOnly("/proc/pressure/cpu");
                if(ff.is_open()){
                    std::string c((std::istreambuf_iterator<char>(ff)), {});
                    float v=0; if(sscanf(c.c_str(),"some avg10=%f",&v)==1) b.cpu.pressureSome10=v;
                } else b.cpu.pressureSome10=-1;
            }
            auto therm = providers::real::RealThermalProvider::getThermals();
            float maxC=0; for(auto &t: therm) if(t.tempC>maxC) maxC=t.tempC;
            b.cpu.thermalMaxC = maxC;
            b.cpu.meta = {"", "%", "/proc/cpuinfo + /sys/devices/system/cpu", "procfs+sysfs", 0.98f, true, ""};
        }
        // Memory
        {
            auto mem = providers::real::RealMemoryProvider::get();
            b.memory.totalKb = mem.totalKb; b.memory.availableKb = mem.availableKb; b.memory.cachedKb = mem.cachedKb;
            b.memory.swappiness = mem.swappiness; b.memory.vfsPressure = mem.vfsCachePressure;
            b.memory.pressureSome10 = mem.pressure.someAvg10; b.memory.pressureFull10 = mem.pressure.fullAvg10;
            b.memory.swapTotalKb = mem.swap.total; b.memory.swapUsedKb = mem.swap.used;
            b.memory.zramDisksize = mem.zram.disksize; b.memory.zramData = mem.zram.data;
            // used = total - available (approx) or from free
            b.memory.usedKb = (mem.totalKb > mem.availableKb ? mem.totalKb - mem.availableKb : 0);
            b.memory.meta = {"", "kB", "/proc/meminfo + /proc/pressure/memory", "procfs", 0.98f, true, ""};
        }
        // Storage
        {
            auto fs = providers::real::RealStorageProvider::getFilesystems();
            for(auto &f: fs){
                domain::StorageBaseline::Fs ff;
                ff.device=f.device; ff.mount=f.mount; ff.fstype=f.fstype; ff.sizeBytes=f.sizeBytes; ff.freeBytes=f.freeBytes;
                ff.usedPct = f.sizeBytes ? 100.0*(double)(f.sizeBytes - f.freeBytes)/f.sizeBytes : 0;
                ff.meta = {"", "bytes", "/proc/mounts+statvfs", "procfs", 0.90f, true, ""};
                b.storage.filesystems.push_back(ff);
            }
            auto blks = providers::real::RealStorageProvider::getBlockDevices();
            for(auto &d: blks){
                domain::StorageBaseline::Dev dev;
                dev.name=d.name; dev.model=d.model; dev.type=d.type; dev.scheduler=d.scheduler; dev.sizeBytes=d.sizeBytes;
                dev.meta = {"", "bytes", "/sys/block", "sysfs", 0.95f, true, ""};
                b.storage.devices.push_back(dev);
            }
            {
                auto f = safety::openReadOnly("/proc/pressure/io");
                if(f.is_open()){
                    std::string c((std::istreambuf_iterator<char>(f)), {});
                    float v=0; if(sscanf(c.c_str(),"some avg10=%f",&v)==1) b.storage.ioPressureSome10=v;
                    if(sscanf(c.c_str(),"%*s %*s full avg10=%f",&v)==1) b.storage.ioPressureFull10=v;
                }
            }
            b.storage.trimEnabled = (access("/run/systemd/system/fstrim.timer",F_OK)==0);
            b.storage.meta = {"", "", "/proc/mounts + /sys/block + /proc/pressure/io", "procfs/sysfs", 0.90f, true, ""};
        }
        // GPU
        {
            auto gpus = providers::real::RealGpuProvider::getGpus();
            for(auto &g: gpus){
                domain::GpuBaseline::Gpu gg;
                gg.vendor=g.vendor; gg.model=g.model; gg.pci=g.pciId; gg.driver=g.driver; gg.claimed=g.claimed;
                // glRenderer via provider (may be skipped)
                if(g.claimed && g.vendor=="Intel"){
                    gg.glRenderer = providers::real::RealGpuProvider::getGlRenderer();
                }
                gg.meta = {"", "", "/sys/bus/pci + glxinfo", "sysfs+exec", 0.90f, true, ""};
                b.gpu.gpus.push_back(gg);
            }
            auto nv = providers::real::RealGpuProvider::getNvidiaState();
            b.gpu.nvidia.moduleLoaded = nv.moduleLoaded;
            b.gpu.nvidia.version = nv.version;
            b.gpu.nvidia.smiAvailable = nv.nvidiaSmiAvailable;
            b.gpu.nvidia.gsp = nv.gspFirmware;
            b.gpu.nvidia.primeState = nv.prime ? "hybrid" : "intel-only";
            b.gpu.nvidia.meta = {"", "", "/sys/module/nvidia + /proc/modules", "procfs", 0.90f, true, ""};
            b.gpu.meta = {"", "", "/sys/bus/pci + glxinfo", "sysfs", 0.85f, true, ""};
        }
        // Thermal
        {
            auto zones = providers::real::RealThermalProvider::getThermals();
            float cpuMax=0, gpuMax=0, nvmeMax=0;
            for(auto &z: zones){
                domain::ThermalBaseline::Zone zz;
                zz.source=z.source; zz.label=z.label; zz.tempC=z.tempC; zz.highC=z.highC; zz.critC=z.critC;
                zz.meta = {"", "C", "/sys/class/hwmon + /sys/class/thermal", "sysfs", 0.90f, true, ""};
                b.thermal.zones.push_back(zz);
                if(z.label.find("coretemp")!=std::string::npos || z.label.find("x86_pkg")!=std::string::npos) cpuMax = std::max(cpuMax, z.tempC);
                if(z.label.find("nvme")!=std::string::npos) nvmeMax = std::max(nvmeMax, z.tempC);
            }
            b.thermal.cpuMaxC = cpuMax;
            b.thermal.nvmeMaxC = nvmeMax;
            b.thermal.gpuMaxC = gpuMax;
            // throttling check via /proc/cpuinfo flags or journal? For now check thermal throttling via dmesg would need privilege - report false unless temp >95
            b.thermal.throttling = (cpuMax>95);
            b.thermal.meta = {"", "C", "/sys/class/hwmon", "sysfs", 0.90f, true, ""};
        }
        // Systemd
        {
            auto boot = providers::real::RealSystemdProvider::getBoot();
            b.systemd.firmware = boot.firmware; b.systemd.loader = boot.loader; b.systemd.kernel = boot.kernel; b.systemd.initrd = boot.initrd; b.systemd.userspace = boot.userspace;
            b.systemd.total = boot.firmware + boot.loader + boot.kernel + boot.initrd + boot.userspace;
            b.systemd.blameTop = boot.blameTop;
            b.systemd.criticalChain = boot.criticalChain;
            auto failed = providers::real::RealSystemdProvider::getFailedServices();
            b.systemd.failedCount = failed.size();
            for(auto &f: failed) b.systemd.failedNames.push_back(f.name);
            // classify: blocker if in critical-chain and >1s, else background
            for(auto &p: boot.blameTop){
                domain::SystemdBaseline::CriticalBlocker cb;
                cb.unit = p.first; cb.sec = p.second;
                bool inChain = boot.criticalChain.find(p.first)!=std::string::npos;
                cb.isBlocker = inChain && p.second>1.0f;
                cb.reason = inChain ? (p.second>1.0f ? "in critical-chain and >1s" : "in critical-chain but <1s") : "parallel/background (not in critical-chain)";
                b.systemd.classified.push_back(cb);
            }
            b.systemd.meta = {"", "s", "systemd-analyze + systemctl --failed", "exec fixed path", 0.97f, true, ""};
        }
        // Processes
        {
            auto top = providers::real::RealProcessProvider::getTop(15);
            b.processes.totalCount = top.size(); // approximate
            for(auto &p: top){
                domain::ProcessBaseline::Proc pr;
                pr.pid=p.pid; pr.name=p.name; pr.rssKb=p.rssKb; pr.cpu=0;
                pr.meta = {"", "kB", "/proc/<pid>/status", "procfs", 0.85f, true, ""};
                b.processes.top.push_back(pr);
            }
            auto f = safety::openReadOnly("/proc/loadavg");
            if(f.is_open()) std::getline(f, b.processes.loadAvg);
            b.processes.meta = {"", "", "/proc", "procfs", 0.85f, true, ""};
        }
        // Journal
        {
            auto p3 = providers::real::RealJournalProvider::getPriorityErrors(3, 20);
            auto cnt = providers::real::RealJournalProvider::countPriority(3);
            auto nv = providers::real::RealJournalProvider::getNvidiaErrors();
            b.journal.p3count = cnt;
            b.journal.p3sample = p3;
            b.journal.nvidiaErrs = nv.size();
            // families grouping
            std::map<std::string,int> fam;
            std::map<std::string,std::string> ex;
            for(auto &line: p3){
                std::string key="other";
                if(line.find("nvidia")!=std::string::npos || line.find("NVRM")!=std::string::npos) key="nvidia";
                else if(line.find("VBoxCreateUSBNode")!=std::string::npos) key="virtualbox-usb";
                else if(line.find("hid-generic")!=std::string::npos) key="hid-generic";
                else if(line.find("ACPI BIOS Error")!=std::string::npos) key="acpi-bios";
                else if(line.find("bluetoothd")!=std::string::npos) key="bluetooth";
                else if(line.find("kvm_amd")!=std::string::npos) key="kvm-amd-on-intel";
                else if(line.find("Timed out waiting for device")!=std::string::npos) key="device-timeout";
                fam[key]++; if(ex[key].empty()) ex[key]=line.substr(0,120);
            }
            for(auto &kv: fam){
                domain::JournalBaseline::Family ff;
                ff.pattern=kv.first; ff.count=kv.second; ff.example=ex[kv.first];
                b.journal.families.push_back(ff);
            }
            b.journal.nvidiaScope = "current boot (-b) sample, historical would be without -b";
            b.journal.meta = {"", "count", "journalctl -p 3 -b", "exec", 0.80f, true, ""};
        }
        // KDE
        {
            b.kde.plasma = providers::real::RealKdeProvider::getPlasmashellVersion();
            const char* s = getenv("XDG_SESSION_TYPE"); b.kde.sessionType = s? s : "unknown";
            s = getenv("WAYLAND_DISPLAY"); b.kde.wayland = s? s : "";
            s = getenv("DISPLAY"); b.kde.display = s? s : "";
            b.kde.effects = providers::real::RealKdeProvider::getDesktop().effects;
            b.kde.meta = {"", "", "env + kwinrc + plasmashell --version", "env+file+exec", 0.90f, true, ""};
        }

        b.meta = {b.timestamp, "", "multiple", "BaselineEngine::collect()", 0.95f, true, ""};
        return b;
    }
};

} // namespace polaris::engines::perf
