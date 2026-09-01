#pragma once
#include "../../domain/PerfModels.h"
#include "../../profile/UserProfile.h"
#include <vector>

namespace polaris::engines::recommend {

class RecommendationEngine {
public:
    static std::vector<domain::Recommendation> generate(const domain::PerformanceBaseline& b, const std::vector<domain::Bottleneck>& bottlenecks);
    // P19: registry-aware generation with profile
    static std::vector<domain::Recommendation> generateWithProfile(const domain::PerformanceBaseline& b, const std::vector<domain::Bottleneck>& bottlenecks, const profile::UserProfile& profile);
    // Enumerate registry capabilities metadata (for CLI)
    static std::vector<std::string> capabilityIds();
};

} // namespace polaris::engines::recommend
