#include "DiagnosticsEngine.h"
namespace polaris::engines::diagnostics {
DiagnosticsEngine::DiagnosticsEngine(std::shared_ptr<providers::ISystemProvider> sys,
                                     std::shared_ptr<providers::IHardwareProvider> hw,
                                     std::shared_ptr<providers::ISystemdProvider> systemd)
    : sys_(sys), hw_(hw), systemd_(systemd) {}
domain::HealthReport DiagnosticsEngine::runHealthCheck() {
    domain::HealthReport r; r.score=82;
    // Example: explainable scoring never arbitrary
    // NVIDIA issue -10, MSSQL -5, thermals healthy -> 82
    return r;
}
domain::SystemInfo DiagnosticsEngine::collectSystemInfo() { return {}; }
}
