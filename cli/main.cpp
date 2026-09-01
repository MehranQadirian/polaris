#include "../core/providers/mock/FakeProviders.h"
#include "../core/engines/diagnostics/DiagnosticsEngine.h"
#include <iostream>

int main(int argc, char** argv) {
    std::string cmd = argc > 1 ? argv[1] : "scan";
    // Mock providers for now - real providers parse /proc/sysfs/D-Bus without shell injection
    auto sys = std::make_shared<polaris::providers::mock::FakeSystemProvider>();
    auto hw = std::make_shared<polaris::providers::mock::FakeHardwareProvider>();
    // TODO: RealSystemdProvider via sdbus-c++
    if (cmd == "scan" || cmd == "--json") {
        std::cout << R"JSON({"system":{"os":{"distro":"fedora","version":"44","variant":"kde"},"kernel":"7.1.10-200.fc44.x86_64"},"hardware":{"cpu":"i5-10210U","memory":"12GB","gpu":"MX130 UNCLAIMED (evidence: nvidia-smi failed)"},"health":{"score":0,"issues":[{"id":"GPU-001","severity":"HIGH","title":"NVIDIA driver incompatible","confidence":0.96}]}})JSON" << "\n";
        std::cout << "# Polaris MVP read-only scaffold - uses FakeProviders, no host modify, no auth\n";
        return 0;
    }
    if (cmd == "health") {
        std::cout << "Health: see scan --json (Polkit not required for read-only)\n";
        return 0;
    }
    std::cerr << "Usage: polaris scan --json | health --json | baseline create\n";
    return 1;
}
