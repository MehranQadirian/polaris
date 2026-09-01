#pragma once
#include "../../domain/HardwareInfo.h"
#include "../../domain/Perf.h"
#include "../../safety/ReadOnlyGuard.h"
#include <fstream>
#include <string>
#include <vector>
#include <sys/statvfs.h>
#include <dirent.h>
#include <unistd.h>

namespace polaris::providers::real {

class RealStorageProvider {
public:
    static std::vector<domain::FilesystemInfo> getFilesystems() {
        std::vector<domain::FilesystemInfo> out;
        auto f = safety::openReadOnly("/proc/mounts");
        if(!f.is_open()) return out;
        std::string line;
        while(std::getline(f,line)){
            char dev[256], mnt[256], fst[64], opts[512];
            if(sscanf(line.c_str(),"%255s %255s %63s %511s",dev,mnt,fst,opts)>=3){
                domain::FilesystemInfo fi;
                fi.device=dev; fi.mount=mnt; fi.fstype=fst; fi.options=opts;
                struct statvfs sv;
                if(statvfs(mnt,&sv)==0){
                    fi.sizeBytes = (uint64_t)sv.f_blocks * sv.f_frsize;
                    fi.freeBytes = (uint64_t)sv.f_bfree * sv.f_frsize;
                    fi.usedBytes = fi.sizeBytes - fi.freeBytes;
                }
                // scheduler via /sys/block/<dev>/queue/scheduler
                std::string devName = dev;
                // extract basename if /dev/nvme0n1p3 -> nvme0n1
                if(devName.rfind("/dev/",0)==0) devName=devName.substr(5);
                // strip partition: nvme0n1p3 -> nvme0n1, sda3 -> sda
                // simple: try direct, else trim trailing digits/p
                auto schedPath = std::string("/sys/block/") + devName + "/queue/scheduler";
                auto ff = safety::openReadOnly(schedPath);
                if(!ff.is_open()){
                    // try parent without partition
                    std::string base=devName;
                    while(!base.empty() && (isdigit(base.back()) || base.back()=='p')) base.pop_back();
                    schedPath = "/sys/block/"+base+"/queue/scheduler";
                    ff = safety::openReadOnly(schedPath);
                }
                if(ff.is_open()){ std::getline(ff, fi.scheduler); }
                out.push_back(fi);
            }
        }
        return out;
    }

    static std::vector<domain::StorageDevice> getBlockDevices() {
        std::vector<domain::StorageDevice> out;
        // Walk /sys/block
        DIR* d = opendir("/sys/block");
        if(!d) return out;
        struct dirent* e;
        while((e=readdir(d))){
            if(e->d_name[0]=='.') continue;
            std::string name=e->d_name; // nvme0n1, sda, zram0, loop0
            if(name.rfind("loop",0)==0) continue;
            if(name.rfind("zram",0)==0) continue; // handled via memory
            domain::StorageDevice sd;
            sd.name=name;
            // model
            {
                auto ff = safety::openReadOnly("/sys/block/"+name+"/device/model");
                if(ff.is_open()) std::getline(ff, sd.model);
                if(sd.model.empty()){
                    auto ff2 = safety::openReadOnly("/sys/block/"+name+"/device/name");
                    if(ff2.is_open()) std::getline(ff2, sd.model);
                }
            }
            // size
            {
                auto ff = safety::openReadOnly("/sys/block/"+name+"/size");
                if(ff.is_open()){ std::string v; ff>>v; try{ uint64_t sectors=stoull(v); sd.sizeBytes=sectors*512; }catch(...){} }
            }
            // type via queue/rotational
            {
                auto ff = safety::openReadOnly("/sys/block/"+name+"/queue/rotational");
                if(ff.is_open()){ std::string v; ff>>v; sd.type = (v=="1" ? "HDD" : "SSD/NVMe"); }
                if(name.rfind("nvme",0)==0) sd.type="NVMe";
            }
            // fstype/mount via filesystems above? fill later
            // scheduler
            {
                auto ff = safety::openReadOnly("/sys/block/"+name+"/queue/scheduler");
                if(ff.is_open()) std::getline(ff, sd.scheduler);
            }
            // smart health - try /sys/class/nvme/nvme0/smart? Permission limited -> report skipped
            // We will attempt to read /sys/class/nvme/nvme0/device/smart? But best effort
            out.push_back(sd);
        }
        closedir(d);
        return out;
    }

    // Try NVMe smart via sysfs without sudo; if permission denied, caller reports skipped.
    static std::optional<std::string> tryNvmeSmart(const std::string& dev="nvme0") {
        auto f = safety::openReadOnly("/sys/class/nvme/"+dev+"/smart_initalized");
        if(!f.is_open()) return std::nullopt;
        std::string c((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return c;
    }
};

} // namespace polaris::providers::real
