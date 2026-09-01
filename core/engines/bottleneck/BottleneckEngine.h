#pragma once
#include "../../domain/PerfModels.h"
#include <vector>
#include <string>

namespace polaris::engines::bottleneck {

class BottleneckEngine {
public:
    // Explainable, multi-evidence, not arbitrary thresholds
    static std::vector<domain::Bottleneck> analyze(const domain::PerformanceBaseline& b);

    // Critical chain blocker vs background classification already in baseline,
    // but expose helper for boot analysis
    static std::string classifyBoot(const domain::PerformanceBaseline& b);
};

} // namespace polaris::engines::bottleneck
