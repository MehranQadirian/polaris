#pragma once
#include "../../domain/PerfModels.h"
#include <vector>

namespace polaris::engines::recommend {

class RecommendationEngine {
public:
    static std::vector<domain::Recommendation> generate(const domain::PerformanceBaseline& b, const std::vector<domain::Bottleneck>& bottlenecks);
};

} // namespace polaris::engines::recommend
