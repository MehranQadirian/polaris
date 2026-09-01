#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <chrono>

namespace polaris::domain {

struct OsInfo {
    std::string distro = "fedora";
    std::string variantId; // kde
    std::string versionId; // 44
    std::string arch = "x86_64";
    std::string prettyName;
};

struct KernelInfo {
    std::string version; // 7.1.10-200.fc44.x86_64
    std::string cmdline;
};

struct DesktopInfo {
    std::string plasma; // 6.7.4
    std::string frameworks;
    std::string sessionType; // wayland/x11
    std::string compositor;
    std::map<std::string,bool> effects; // blur->true, glide->true
    struct Autostart { std::string name, path; bool hidden; };
    std::vector<Autostart> autostart;
};

struct SystemInfo {
    OsInfo os;
    KernelInfo kernel;
    DesktopInfo desktop;
    std::chrono::system_clock::time_point timestamp;
};

} // namespace polaris::domain
