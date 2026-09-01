#pragma once
#include "../domain/PerfModels.h"
#include "../domain/Comparison.h"
#include "../profile/UserProfile.h"
#include "../safety/transaction/Transaction.h"
#include "../safety/transaction/TransactionValidator.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace polaris::capabilities {

struct CapabilityEvidence {
    bool available = false;
    std::string reason; // if not available, human
    std::vector<std::string> evidence; // sorted for determinism
    double confidence = 0.0; // 0-1
    double benefitGB = 0.0; // numeric, 0 if unavailable
    uint64_t reclaimableBytes = 0;
    std::string benefitStr; // human
    std::string risk; // R0-3
    std::map<std::string,std::string> preconditions; // for stale
    std::string stateHash; // hash for stale (deterministic)
    bool isDeterministic = true;
};

class IOptimizationCapability {
public:
    virtual ~IOptimizationCapability() = default;
    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string category() const = 0;
    virtual std::string description() const = 0;
    virtual std::string risk() const = 0; // R0, R1, R2, R3
    virtual std::string reversibility() const = 0;
    virtual bool requiresReboot() const = 0;
    virtual bool requiresAuth() const = 0;
    // Whether capability can be considered given baseline + profile
    // This does NOT authorize mutation, only analysis.
    virtual bool isApplicable(const domain::PerformanceBaseline& b, const profile::UserProfile& profile) const = 0;
    virtual CapabilityEvidence collect(const domain::PerformanceBaseline& b) const = 0;
    virtual domain::Recommendation toRecommendation(const CapabilityEvidence& ev, const domain::PerformanceBaseline& b) const = 0;
    virtual safety::CurrentState snapshot(const domain::PerformanceBaseline& b, const CapabilityEvidence& ev) const = 0;
    virtual safety::Transaction toTransaction(const std::string& txId, const domain::Recommendation& rec, const CapabilityEvidence& ev, const safety::CurrentState& cur) const = 0;
    // Verify benefit: returns true if verification executed, sets observedBenefit, verdict, details. If metrics unavailable -> INCONCLUSIVE.
    virtual bool verify(const domain::PerformanceBaseline& before, const domain::PerformanceBaseline& after, std::string& observedBenefit, domain::Verdict& verdict, std::string& details) const = 0;
    // Explainability hooks
    virtual std::string explainWhyNow(const domain::PerformanceBaseline& b, const CapabilityEvidence& ev, const profile::UserProfile& profile) const = 0;
    virtual std::string explainWhatWillChange(const CapabilityEvidence& ev) const = 0;
    virtual std::string explainWhatWillNotChange() const = 0;
    virtual std::vector<std::string> rejectionConditions(const domain::PerformanceBaseline& b, const CapabilityEvidence& ev, const profile::UserProfile& profile) const = 0;
};

} // namespace polaris::capabilities
