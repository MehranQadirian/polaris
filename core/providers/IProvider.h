#pragma once
#include "../domain/SystemInfo.h"
#include "../domain/HardwareInfo.h"
#include "../domain/Health.h"
#include <string>
#include <vector>
#include <optional>

namespace polaris::providers {

// All providers are read-only for MVP. Mutating providers go via Safety layer.

struct ISystemProvider {
    virtual ~ISystemProvider() = default;
    virtual domain::OsInfo getOs() = 0;
    virtual domain::KernelInfo getKernel() = 0;
    virtual domain::DesktopInfo getDesktop() = 0;
};

struct IHardwareProvider {
    virtual ~IHardwareProvider() = default;
    virtual domain::CpuInfo getCpu() = 0;
    virtual domain::MemoryInfo getMemory() = 0;
    virtual std::vector<domain::StorageDevice> getStorage() = 0;
    virtual std::vector<domain::GpuInfo> getGpus() = 0;
};

struct ISystemdProvider {
    virtual ~ISystemdProvider() = default;
    struct ServiceTime { std::string name; float seconds; bool failed; };
    virtual std::vector<ServiceTime> blame() = 0;
    virtual std::vector<std::string> failedUnits() = 0;
    struct BootTime { float firmware, loader, kernel, initrd, userspace; };
    virtual BootTime analyze() = 0;
    virtual std::string criticalChain() = 0;
};

struct IJournalProvider {
    virtual ~IJournalProvider() = default;
    virtual std::vector<std::string> errorsSinceBoot(int priority=3) = 0; // p3
    virtual std::vector<std::string> nvidiaErrors() = 0;
};

} // namespace polaris::providers
