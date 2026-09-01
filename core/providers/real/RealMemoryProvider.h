#pragma once
#include "../../domain/HardwareInfo.h"
#include "../../safety/ReadOnlyGuard.h"
#include <fstream>
#include <string>

namespace polaris::providers::real {

class RealMemoryProvider {
public:
    static domain::MemoryInfo get() {
        domain::MemoryInfo m;
        auto f = safety::openReadOnly("/proc/meminfo");
        if(f.is_open()){
            std::string line;
            while(std::getline(f,line)){
                auto parse = [&](const std::string& key, uint64_t& out){
                    if(line.rfind(key,0)==0){
                        auto col=line.find(':'); if(col==std::string::npos) return;
                        std::string v=line.substr(col+1);
                        // "       11968360 kB"
                        uint64_t val=0; sscanf(v.c_str(),"%lu",&val);
                        out=val;
                    }
                };
                parse("MemTotal:", m.totalKb);
                parse("MemAvailable:", m.availableKb);
                parse("Cached:", m.cachedKb);
            }
        }
        // /proc/pressure/memory
        {
            auto ff = safety::openReadOnly("/proc/pressure/memory");
            if(ff.is_open()){
                std::string content((std::istreambuf_iterator<char>(ff)), std::istreambuf_iterator<char>());
                // some avg10=0.00 total=...
                auto findAvg = [&](const std::string& prefix)->float{
                    auto pos=content.find(prefix);
                    if(pos==std::string::npos) return 0;
                    float v=0; sscanf(content.c_str()+pos+prefix.size(),"%f",&v); return v;
                };
                m.pressure.someAvg10 = findAvg("some avg10=");
                m.pressure.fullAvg10 = findAvg("full avg10=");
            }
        }
        // swappiness
        {
            auto ff = safety::openReadOnly("/proc/sys/vm/swappiness");
            if(ff.is_open()){ std::string v; ff>>v; try{m.swappiness=std::stof(v);}catch(...){} }
        }
        {
            auto ff = safety::openReadOnly("/proc/sys/vm/vfs_cache_pressure");
            if(ff.is_open()){ std::string v; ff>>v; try{m.vfsCachePressure=std::stof(v);}catch(...){} }
        }
        // swaps
        {
            auto ff = safety::openReadOnly("/proc/swaps");
            if(ff.is_open()){
                std::string line; std::getline(ff,line); // header
                if(std::getline(ff,line)){
                    // /dev/zram0 partition 8388604 0 100
                    char dev[128]; unsigned long long sz=0, used=0;
                    if(sscanf(line.c_str(),"%127s %*s %llu %llu",dev,&sz,&used)==3){
                        m.swap.total = sz; m.swap.used = used;
                    }
                }
            }
        }
        // zram
        {
            auto ff = safety::openReadOnly("/sys/block/zram0/mm_stat");
            if(ff.is_open()){
                std::string c((std::istreambuf_iterator<char>(ff)), std::istreambuf_iterator<char>());
                // 4096 80 12288 ...
                unsigned long long orig=0; sscanf(c.c_str(),"%llu",&orig); m.zram.data=orig;
            }
            auto ff2 = safety::openReadOnly("/sys/block/zram0/comp_algorithm");
            if(ff2.is_open()){ std::string algo; std::getline(ff2,algo); m.zram.algo=algo; }
            auto ff3 = safety::openReadOnly("/sys/block/zram0/disksize");
            if(ff3.is_open()){ std::string v; ff3>>v; try{m.zram.disksize=stoull(v);}catch(...){} }
        }
        // fallback zram disksize via /sys
        return m;
    }
};

} // namespace polaris::providers::real
