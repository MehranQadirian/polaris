#include "../../core/providers/real/RealOsProvider.h"
#include "../../core/providers/real/RealCpuProvider.h"
#include "../../core/providers/real/RealMemoryProvider.h"
#include "../../core/providers/real/RealStorageProvider.h"
#include "../../core/providers/real/RealThermalProvider.h"
#include "../../core/providers/real/RealGpuProvider.h"
#include <cassert>
#include <iostream>

int main(){
    // OS
    auto os = polaris::providers::real::RealOsProvider::getOs();
    std::cout << "OS: " << os.prettyName << " variant=" << os.variantId << " arch=" << os.arch << "\n";
    assert(!os.prettyName.empty());
    assert(os.arch=="x86_64" || os.arch=="aarch64");

    // Kernel
    auto k = polaris::providers::real::RealOsProvider::getKernel();
    std::cout << "Kernel: " << k.version << "\n";
    assert(!k.version.empty());

    // CPU
    auto cpu = polaris::providers::real::RealCpuProvider::getCpu();
    std::cout << "CPU: " << cpu.model << " cores=" << cpu.cores << " threads=" << cpu.threads << " gov=" << cpu.governor << "\n";
    assert(!cpu.model.empty());
    assert(cpu.cores>0);
    assert(cpu.threads>=cpu.cores);

    // Memory
    auto mem = polaris::providers::real::RealMemoryProvider::get();
    std::cout << "Mem totalKb=" << mem.totalKb << " avail=" << mem.availableKb << " swappiness=" << mem.swappiness << "\n";
    assert(mem.totalKb>0);
    assert(mem.swappiness>=0 && mem.swappiness<=100);

    // Storage filesystems
    auto fs = polaris::providers::real::RealStorageProvider::getFilesystems();
    std::cout << "Filesystems: " << fs.size() << " mounts\n";
    assert(fs.size()>0);
    bool hasRoot=false;
    for(auto &f: fs) if(f.mount=="/") hasRoot=true;
    (void)hasRoot; assert(hasRoot);

    // Block devices
    auto blks = polaris::providers::real::RealStorageProvider::getBlockDevices();
    std::cout << "Block devices: " << blks.size() << "\n";
    // Should have at least nvme or sda
    assert(blks.size()>0);

    // Thermal (may be empty in container but should not crash)
    auto therm = polaris::providers::real::RealThermalProvider::getThermals();
    std::cout << "Thermals: " << therm.size() << " sensors\n";

    // GPU
    auto gpus = polaris::providers::real::RealGpuProvider::getGpus();
    std::cout << "GPUs: " << gpus.size() << "\n";
    for(auto &g: gpus) std::cout << "  " << g.vendor << " " << g.model << " driver=" << g.driver << " claimed=" << g.claimed << "\n";

    auto nvidia = polaris::providers::real::RealGpuProvider::getNvidiaState();
    std::cout << "Nvidia moduleLoaded=" << nvidia.moduleLoaded << " version=" << nvidia.version << "\n";

    std::cout << "P2 read-only unit tests passed\n";
    return 0;
}
