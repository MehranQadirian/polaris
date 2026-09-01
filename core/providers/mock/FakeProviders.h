#pragma once
#include "../IProvider.h"

namespace polaris::providers::mock {

// Simulation: returns reference machine i5-10210U/12GB/MX130 without touching real host. For tests.

class FakeSystemProvider : public ISystemProvider {
public:
    domain::OsInfo getOs() override {
        return {.distro="fedora", .variantId="kde", .versionId="44", .arch="x86_64", .prettyName="Fedora Linux 44 (KDE Plasma Desktop Edition)"};
    }
    domain::KernelInfo getKernel() override {
        return {.version="7.1.10-200.fc44.x86_64", .cmdline="BOOT_IMAGE=(hd1,gpt3)/... rhgb quiet"};
    }
    domain::DesktopInfo getDesktop() override {
        domain::DesktopInfo d; d.plasma="6.7.4"; d.sessionType="wayland"; d.effects={{"blur",true},{"glide",true}}; return d;
    }
};

class FakeHardwareProvider : public IHardwareProvider {
public:
    domain::CpuInfo getCpu() override { domain::CpuInfo c; c.model="Intel(R) Core(TM) i5-10210U"; c.cores=4; c.threads=8; c.scalingDriver="intel_pstate"; c.governor="powersave"; c.epp="balance_performance"; c.freqMaxMhz=4200; c.tempC=54; return c; }
    domain::MemoryInfo getMemory() override { domain::MemoryInfo m; m.totalKb=11968360; m.swappiness=60; m.zram={"lzo-rle", 8ULL<<30, 4096, 80}; return m; }
    std::vector<domain::StorageDevice> getStorage() override { return { {"nvme0n1","WD Green SN3000","nvme","ext4","/", 343ULL<<30, 103ULL<<30, "[none]", {true,3}, true}}; }
    std::vector<domain::GpuInfo> getGpus() override { return { {"Intel","UHD Graphics CML GT2","8086:9b41","i915","i915",true,"Mesa Intel","vulkan-intel",0,0,0,"",false}, {"NVIDIA","GM108M MX130","10de:174d","nvidia","nvidia",false,"","",0,0,0,"",false} }; }
};

} // namespace polaris::providers::mock
