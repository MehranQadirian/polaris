#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace polaris::domain {

struct ThermalInfo {
    std::string source; // hwmon0, thermal_zone0
    std::string label;
    float tempC=0;
    float highC=0, critC=0;
};

struct ProcessInfo {
    int pid=0;
    std::string name, user;
    float cpu=0, mem=0;
    uint64_t rssKb=0, vszKb=0;
    std::string state;
};

struct ServiceInfo {
    std::string name;
    std::string load, active, sub;
    std::string description;
    bool enabled=false;
    bool failed=false;
    float startSec=0; // from blame
};

struct BootInfo {
    float firmware=0, loader=0, kernel=0, initrd=0, userspace=0;
    std::vector<std::pair<std::string,float>> blameTop; // name, seconds
    std::string criticalChain;
};

struct FilesystemInfo {
    std::string device, mount, fstype, options;
    uint64_t sizeBytes=0, usedBytes=0, freeBytes=0;
    std::string scheduler;
};

} // namespace polaris::domain
