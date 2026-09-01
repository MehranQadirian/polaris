#pragma once
#include "../../domain/Comparison.h"
#include "../../domain/PerfModels.h"
#include <string>
#include <vector>

namespace polaris::engines::comparison {

class ComparisonEngine {
public:
    // Pure, side-effect-free, deterministic, serializable
    // Does not execute commands - providers remain responsible for measurement collection
    static domain::Comparison compare(
        const domain::PerformanceBaseline& before,
        const domain::PerformanceBaseline& after,
        const std::string& expectedBenefit,
        const std::string& transactionId = "CMP-TEST"
    );

    // Explicit thresholds (stored with result, explainable)
    struct Thresholds {
        double bootPct = 10.0; // > +10% relative
        double availableMemGb = 1.0; // > 1 GiB decrease
        double thermalC = 15.0; // > +15°C
        bool failedUnitsNew = true; // any new failed unit
    };

    static const Thresholds& defaultThresholds();

private:
    static domain::MetricComparison makeMetric(
        const std::string& metric,
        std::optional<double> before,
        std::optional<double> after,
        const std::string& thresholdDesc,
        double thresholdValue,
        const std::string& thresholdType,
        bool isBootCritical,
        bool isHealth,
        double confidence
    );
};

} // namespace polaris::engines::comparison
