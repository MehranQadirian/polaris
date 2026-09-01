#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <cstdint>
#include <map>

namespace polaris::domain {

struct MetricMeta {
    std::string timestamp; // ISO8601
    std::string unit;
    std::string source; // e.g., "/proc/meminfo"
    std::string method; // "procfs read"
    float confidence=0.99f;
    bool available=true;
    std::string note; // if unavailable
};

struct CpuBaseline {
    std::string model; // e.g., Intel i5-10210U
    int cores=0, threads=0;
    float utilization=0; // % idle derived from /proc/stat delta
    std::string loadAvg; // "1.84 2.10..."
    float load1=0;
    int curMhz=0, minMhz=0, maxMhz=0;
    std::string governor, epp, driver;
    bool noTurbo=false;
    float pressureSome10=0;
    float thermalMaxC=0;
    MetricMeta meta;
};

struct MemoryBaseline {
    uint64_t totalKb=0, usedKb=0, availableKb=0, cachedKb=0;
    uint64_t swapTotalKb=0, swapUsedKb=0;
    uint64_t zramDisksize=0, zramData=0;
    float swappiness=0, vfsPressure=0;
    float pressureSome10=0, pressureFull10=0;
    float majorFaultsPerSec=0;
    MetricMeta meta;
};

struct StorageBaseline {
    struct Fs { std::string device,mount,fstype; uint64_t sizeBytes, freeBytes; double usedPct; MetricMeta meta; };
    std::vector<Fs> filesystems;
    struct Dev { std::string name,model,type, scheduler; uint64_t sizeBytes; MetricMeta meta; };
    std::vector<Dev> devices;
    float ioPressureSome10=0, ioPressureFull10=0;
    bool trimEnabled=false;
    MetricMeta meta;
};

struct GpuBaseline {
    struct Gpu { std::string vendor, model, pci, driver; bool claimed; std::string glRenderer; std::string vulkan; bool prime; float util=0, vramMB=0, tempC=0; MetricMeta meta; };
    std::vector<Gpu> gpus;
    // NVIDIA specific
    struct Nvidia { bool moduleLoaded=false; std::string version; bool smiAvailable=false; bool gsp=false; std::string primeState; MetricMeta meta; } nvidia;
    MetricMeta meta;
};

struct ThermalBaseline {
    struct Zone { std::string source, label; float tempC, highC, critC; MetricMeta meta; };
    std::vector<Zone> zones;
    float cpuMaxC=0, gpuMaxC=0, nvmeMaxC=0;
    bool throttling=false;
    MetricMeta meta;
};

struct SystemdBaseline {
    float firmware=0, loader=0, kernel=0, initrd=0, userspace=0, total=0;
    std::vector<std::pair<std::string,float>> blameTop;
    std::string criticalChain;
    int failedCount=0;
    std::vector<std::string> failedNames;
    // classified
    struct CriticalBlocker { std::string unit; float sec; bool isBlocker; std::string reason; };
    std::vector<CriticalBlocker> classified;
    MetricMeta meta;
};

struct ProcessBaseline {
    int totalCount=0;
    std::vector<std::string> topCpuNames; // placeholder
    struct Proc { int pid; std::string name; uint64_t rssKb; float cpu=0; MetricMeta meta; };
    std::vector<Proc> top;
    std::string loadAvg;
    MetricMeta meta;
};

struct JournalBaseline {
    int p3count=0;
    std::vector<std::string> p3sample;
    struct Family { std::string pattern; int count; std::string example; };
    std::vector<Family> families;
    int nvidiaErrs=0;
    std::string nvidiaScope; // "current boot" vs "historical"
    MetricMeta meta;
};

struct KdeBaseline {
    std::string plasma, sessionType, wayland, display;
    std::map<std::string,bool> effects;
    std::string compositor;
    MetricMeta meta;
};

struct FlatpakBaseline {
    struct Runtime { std::string id; std::string branch; std::string origin; uint64_t installedSizeBytes=0; bool isRuntime=true; std::string name; };
    std::vector<Runtime> runtimes;
    std::vector<Runtime> unusedRuntimes;
    uint64_t reclaimableBytes=0;
    int totalCount=0;
    bool hasFlatpak=false;
    MetricMeta meta;
};

struct JournalDiskBaseline {
    uint64_t diskUsageBytes=0;
    uint64_t maxUsageBytes=0;
    uint64_t reclaimableBytes=0;
    std::string vacuumTarget; // e.g., "500M" or "14d"
    MetricMeta meta;
};

struct PerformanceBaseline {
    std::string id; // e.g., 2026-08-31T21:50+0330
    std::string timestamp;
    CpuBaseline cpu;
    MemoryBaseline memory;
    StorageBaseline storage;
    GpuBaseline gpu;
    ThermalBaseline thermal;
    SystemdBaseline systemd;
    ProcessBaseline processes;
    JournalBaseline journal;
    KdeBaseline kde;
    FlatpakBaseline flatpak;
    JournalDiskBaseline journalDisk;
    MetricMeta meta;
};

struct Bottleneck {
    std::string id; // BOOT-001
    std::string category; // Boot, GPU, Memory, etc.
    std::string title;
    std::string severity; // LOW,MEDIUM,HIGH,CRITICAL
    float confidence=0; // 0-1
    std::vector<std::string> evidence;
    std::string observedValue;
    std::string expectedValue;
    std::string impact;
    std::string possibleCause;
    std::string investigation;
    std::string potentialOptimization;
    std::string risk; // R0-3
};

struct BenchmarkResult {
    std::string mode; // quick, normal, deep
    std::string name;
    double min=0, max=0, avg=0, median=0, stddev=0;
    std::string unit;
    int runs=0;
    bool cancelled=false;
    MetricMeta meta;
};

struct Recommendation {
    std::string id; // REC-001
    std::string title;
    std::string problem;
    std::vector<std::string> evidence;
    float confidence=0;
    std::string expectedBenefit;
    std::string riskLevel; // 0-3
    std::string affectedComponent;
    std::string why;
    std::string alternative;
    std::string rollbackConcept;
    bool requiresReboot=false, requiresAuth=false, requiresApproval=false;
    std::string category;
};

} // namespace polaris::domain
