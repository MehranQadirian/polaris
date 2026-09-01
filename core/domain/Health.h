#pragma once
#include <string>
#include <vector>

namespace polaris::domain {

enum class Category { Hardware, CPU, Memory, Storage, GPU, Driver, Boot, Service, KDE, Network, Thermal, Security };
enum class Severity { INFO, LOW, MEDIUM, HIGH, CRITICAL };

struct Evidence {
    std::string source; // e.g., "journal: NVRM not supported"
    std::string command; // e.g., "nvidia-smi"
    std::string outputHash;
};

struct HealthIssue {
    std::string id; // GPU-001
    Category category;
    Severity severity;
    std::string title;
    std::string description;
    std::vector<std::string> evidence;
    float confidence=0; // 0-1
    std::string impact;
    std::string recommendation;
    int risk=0; // 0-4
    bool rollbackAvailable=false;
    bool requiresReboot=false;
    bool requiresAuth=false;
};

struct HealthReport {
    int score=100; // 0-100 explainable
    std::vector<HealthIssue> issues;
    std::string summary;
};

} // namespace polaris::domain
