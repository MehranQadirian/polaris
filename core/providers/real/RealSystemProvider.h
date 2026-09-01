#pragma once
#include "../IProvider.h"
#include <string>

namespace polaris::providers::real {

// Real implementations parse /etc/os-release, /proc, sysfs, D-Bus.
// NO shell injection: fixed paths, separate args, validated.

class RealSystemProvider : public ISystemProvider {
public:
    domain::OsInfo getOs() override;
    domain::KernelInfo getKernel() override;
    domain::DesktopInfo getDesktop() override;
};

class RealHardwareProvider : public IHardwareProvider {
public:
    domain::CpuInfo getCpu() override; // parses /proc/cpuinfo, /sys/devices/system/cpu/*
    domain::MemoryInfo getMemory() override; // /proc/meminfo, /proc/pressure/memory, zramctl via lib
    std::vector<domain::StorageDevice> getStorage() override; // lsblk via libblkid, smartctl via API not shell
    std::vector<domain::GpuInfo> getGpus() override; // lspci via libpci, glxinfo via EGL query
};

class RealSystemdProvider : public ISystemdProvider {
public:
    std::vector<ServiceTime> blame() override; // sd-bus call to systemd-analyze API, not shell
    std::vector<std::string> failedUnits() override;
    BootTime analyze() override;
    std::string criticalChain() override;
};

} // namespace polaris::providers::real
