#pragma once
#include "../../providers/IProvider.h"
#include "../../domain/Health.h"
#include <memory>
#include <vector>

namespace polaris::engines::diagnostics {

class DiagnosticsEngine {
public:
    DiagnosticsEngine(std::shared_ptr<providers::ISystemProvider> sys,
                      std::shared_ptr<providers::IHardwareProvider> hw,
                      std::shared_ptr<providers::ISystemdProvider> systemd);

    // Read-only fusion
    domain::HealthReport runHealthCheck();
    domain::SystemInfo collectSystemInfo();
    // Each sub-engine is testable via mocks
private:
    std::shared_ptr<providers::ISystemProvider> sys_;
    std::shared_ptr<providers::IHardwareProvider> hw_;
    std::shared_ptr<providers::ISystemdProvider> systemd_;
};

} // namespace polaris::engines::diagnostics
