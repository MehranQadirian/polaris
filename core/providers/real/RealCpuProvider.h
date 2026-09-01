#pragma once
#include "../../domain/HardwareInfo.h"
#include "../../safety/ReadOnlyGuard.h"
#include <fstream>
#include <string>
#include <vector>
#include <optional>
#include <dirent.h>

namespace polaris::providers::real {

class RealCpuProvider {
public:
    static domain::CpuInfo getCpu() {
        domain::CpuInfo cpu;
        // /proc/cpuinfo
        auto f = safety::openReadOnly("/proc/cpuinfo");
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                if (line.rfind("model name",0)==0) {
                    auto col = line.find(':');
                    if(col!=std::string::npos) cpu.model = line.substr(col+2);
                }
                if (line.rfind("cpu cores",0)==0) {
                    auto col=line.find(':'); if(col!=std::string::npos) cpu.cores = stoi(line.substr(col+1));
                }
            }
        }
        // threads via /proc/cpuinfo count
        {
            auto ff = safety::openReadOnly("/proc/cpuinfo");
            int cnt=0; std::string l; while(std::getline(ff,l)) if(l.rfind("processor",0)==0) cnt++;
            cpu.threads = cnt;
        }
        // /sys/devices/system/cpu/cpu0/cpufreq/*
        auto readSys = [](const std::string& p, std::string& out)->bool{
            auto ff = safety::openReadOnly(p);
            if(!ff.is_open()) return false;
            std::getline(ff, out);
            if(!out.empty() && out.back()=='\n') out.pop_back();
            return true;
        };
        std::string v;
        if(readSys("/sys/devices/system/cpu/cpu0/cpufreq/scaling_driver", v)) cpu.scalingDriver=v;
        if(readSys("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", v)) cpu.governor=v;
        if(readSys("/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference", v)) cpu.epp=v;
        if(readSys("/sys/devices/system/cpu/intel_pstate/status", v)) {} // status
        if(readSys("/sys/devices/system/cpu/intel_pstate/no_turbo", v)) cpu.noTurbo = (v=="1");
        // freq limits
        if(readSys("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", v)) { try{ cpu.freqMaxMhz = stoi(v)/1000; }catch(...){} }
        if(readSys("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq", v)) { try{ cpu.freqMinMhz = stoi(v)/1000; }catch(...){} }
        if(readSys("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", v)) { try{ cpu.curMhz = stoi(v)/1000; }catch(...){} }
        else {
            // fallback /proc/cpuinfo cpu MHz
            auto ff = safety::openReadOnly("/proc/cpuinfo");
            std::string line; while(std::getline(ff,line)) if(line.rfind("cpu MHz",0)==0){ auto col=line.find(':'); if(col!=std::string::npos){ try{cpu.curMhz = (int)stof(line.substr(col+1)); }catch(...){} break;}}
        }
        // temp via hwmon or thermal_zone
        cpu.tempC = 0; // filled by thermal provider fusion, keep 0 here
        return cpu;
    }
};

} // namespace polaris::providers::real
