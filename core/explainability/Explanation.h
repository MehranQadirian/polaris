#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace polaris::explainability {

enum class CandidateKind { RECOMMENDATION, TRANSACTION, PROFILE_CONSTRAINT };
enum class DecisionKind { RECOMMEND, REQUIRE_CONFIRMATION, BLOCKED, PREVIEWED, APPROVED, FAILED, COMPLETED, REGRESSION, NO_CHANGE };

inline std::string toString(CandidateKind k){
    switch(k){
        case CandidateKind::RECOMMENDATION: return "RECOMMENDATION";
        case CandidateKind::TRANSACTION: return "TRANSACTION";
        case CandidateKind::PROFILE_CONSTRAINT: return "PROFILE_CONSTRAINT";
    }
    return "UNKNOWN";
}
inline std::string toString(DecisionKind d){
    switch(d){
        case DecisionKind::RECOMMEND: return "RECOMMEND";
        case DecisionKind::REQUIRE_CONFIRMATION: return "REQUIRE_CONFIRMATION";
        case DecisionKind::BLOCKED: return "BLOCKED";
        case DecisionKind::PREVIEWED: return "PREVIEWED";
        case DecisionKind::APPROVED: return "APPROVED";
        case DecisionKind::FAILED: return "FAILED";
        case DecisionKind::COMPLETED: return "COMPLETED";
        case DecisionKind::REGRESSION: return "REGRESSION";
        case DecisionKind::NO_CHANGE: return "NO_CHANGE";
    }
    return "UNKNOWN";
}

struct Explanation {
    std::string id; // e.g., "EXP-akonadi-001" deterministic from candidateId
    std::string candidateId; // e.g., "akonadi-disable" or "TX-TEST-001"
    CandidateKind candidateKind = CandidateKind::RECOMMENDATION;
    DecisionKind decision = DecisionKind::RECOMMEND;
    std::string decisionLabel; // e.g., "BLOCKED_BY_USER_WORKFLOW", "REQUIRES_USER_CONFIRMATION", "PREVIEWED", "FAILED: stale beforeHash", "COMPLETED: SUCCESS"
    std::string whyNow; // deterministic, backed by evidence
    std::vector<std::string> evidence; // from Recommendation.evidence + ProfileAdvisor + Comparison metrics (sorted for determinism)
    std::string expectedBenefit; // from Recommendation.expectedBenefit
    double confidence = 0.0; // e.g., 0.65
    std::string risk; // R0-R3
    std::string reversibility; // e.g., "High (akonadictl start)"
    bool rebootRequired = false;
    bool authorizationRequired = false;
    std::string userImpact; // e.g., "KMail would lose PIM if Akonadi disabled"
    std::string whatWillChange; // exact transaction scope
    std::string whatWillNotChange; // explicit invariants, scope-aware
    std::vector<std::string> rejectionConditions; // deterministic sorted: "stale beforeHash expected ... observed ..."
    std::vector<std::string> dependencies; // e.g., "requires akonadi running"
    std::string rollbackSummary; // e.g., "Restore from backup /tmp/.../fstab.bak, systemctl enable akonadi"
    std::string beforeStateSummary; // e.g., "Akonadi 14 agents 1302M, failed 0, userspace 8.515s"
    std::string afterStateSummary; // e.g., after apply (if available)
    std::string observedBenefit; // from Comparison.observedBenefit (if available)
    std::string verdict; // from Comparison.toString(verdict) if available
    std::string verdictReason; // from Comparison
    bool hasRegression = false;
    std::string limitations; // e.g., "Comparison unavailable: reboot-pending"

    // Deterministic helpers
    bool isDeterministic = true;

    std::string toJson() const;
    static Explanation fromJson(const std::string& json);
    std::string toHuman(bool verbose) const;
};

// Helpers for redaction
bool containsSecret(const std::string& s);
std::string redact(const std::string& s);

} // namespace polaris::explainability
