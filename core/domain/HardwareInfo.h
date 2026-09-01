#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace polaris::domain {

struct CpuInfo {
    std::string model; // Intel(R) Core(TM) i5-10210U
    int family=0, modelId=0, stepping=0;
    uint32_t cores=0, threads=0;
    std::string scalingDriver; // intel_pstate
    std::string governor; // powersave
    std::string epp; // balance_performance
    bool boost=true;
    bool noTurbo=false;
    uint32_t freqMinMhz=0, freqMaxMhz=0, curMhz=0;
    float tempC=0;
    bool throttling=false;
};

struct MemoryInfo {
    uint64_t totalKb=0, availableKb=0, cachedKb=0;
    float swappiness=60;
    float vfsCachePressure=100;
    struct ZramInfo { std::string algo="lzo-rle"; uint64_t disksize=0, data=0, compr=0; } zram;
    struct SwapInfo { uint64_t total=0, used=0; } swap;
    struct Pressure { float someAvg10=0, fullAvg10=0; uint64_t total=0; } pressure;
};

struct StorageDevice {
    std::string name, model, type; // nvme/sata
    std::string fstype, mountpoint;
    uint64_t sizeBytes=0, freeBytes=0;
    std::string scheduler; // [none] / bfq
    struct Smart { bool passed=true; int pctUsed=0; uint64_t readTB=0, writtenTB=0; int tempC=0; } smart;
    bool trimEnabled=false;
};

struct GpuInfo {
    std::string vendor, model, pciId; // 10de:174d
    std::string driver, module; // i915/nvidia/nouveau
    bool claimed=false;
    std::string glRenderer, vulkan;
    float util=0, vramUsedMB=0, tempC=0;
    std::string powerState;
    bool prime=false;
};

struct HardwareInfo {
    CpuInfo cpu;
    MemoryInfo memory;
    std::vector<StorageDevice> storage;
    std::vector<GpuInfo> gpus;
    std::vector<std::string> sensors;
};

} // namespace polaris::domain
