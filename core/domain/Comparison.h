#pragma once
#include "PerfModels.h"
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace polaris::domain {

// P11: Structured post-change measurement - deterministic, serializable, explainable

enum class Verdict {
    SUCCESS,        // observed benefit matches expected, no regression
    IMPROVED,       // benefit observed, but not exactly as expected (still positive)
    NO_CHANGE,      // idempotent, no delta
    NO_BENEFIT,     // operation succeeded but no benefit observed
    REGRESSION,     // critical metric regressed
    INCONCLUSIVE    // insufficient data (unavailable metrics)
};

inline std::string toString(Verdict v){
    switch(v){
        case Verdict::SUCCESS: return "SUCCESS";
        case Verdict::IMPROVED: return "IMPROVED";
        case Verdict::NO_CHANGE: return "NO_CHANGE";
        case Verdict::NO_BENEFIT: return "NO_BENEFIT";
        case Verdict::REGRESSION: return "REGRESSION";
        case Verdict::INCONCLUSIVE: return "INCONCLUSIVE";
    }
    return "UNKNOWN";
}

struct MetricComparison {
    std::string metric; // e.g., "boot.userspace", "memory.available", "thermal.cpuMax", "systemd.failedCount", "nvidia.claimed"
    std::optional<double> before; // nullopt => unavailable
    std::optional<double> after;  // nullopt => unavailable
    std::optional<double> delta; // after - before if both available
    std::optional<double> pctDelta; // delta/before*100 if before !=0 and available
    bool available = true; // false if before or after unavailable
    std::string note; // if unavailable: "unavailable: no nvidia-smi"
    double confidence = 0.95; // per metric
    bool isBootCritical = false; // true for boot time, false for background
    bool isBackground = false;
    bool isHealth = false; // true for failed units, thermal, etc.
    // Regression threshold (stored with result, explainable)
    std::string thresholdDesc; // e.g., "boot > +10% relative"
    double thresholdValue = 0; // e.g., 10.0 for 10%
    std::string thresholdType; // "relative_pct", "absolute_gb", "absolute_c", "new_failed"
    bool regression = false; // true if threshold exceeded
};

struct Comparison {
    std::string id; // e.g., CMP-20260831-001
    std::string transactionId;
    std::string beforeTimestamp;
    std::string afterTimestamp;
    PerformanceBaseline beforeBaseline;
    PerformanceBaseline afterBaseline;
    std::vector<MetricComparison> metrics;
    std::string expectedBenefit; // e.g., "restore NVIDIA Maxwell support and PRIME offload"
    std::string observedBenefit; // e.g., "MX130 claimed, nvidia module loaded, nvidia-smi successful, PRIME offload successful"
    Verdict verdict = Verdict::INCONCLUSIVE;
    std::string verdictReason; // explain why
    bool hasRegression = false; // true if any critical metric regressed
    std::string rebootMarker; // "none", "reboot-pending", "rebooted-2026-09-01T00:36"
    std::string loginMarker; // similar for login-required
    bool isDeterministic = true; // always true for this engine
};

} // namespace polaris::domain
