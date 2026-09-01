#pragma once
#include "Explanation.h"
#include "../domain/PerfModels.h"
#include "../safety/transaction/Transaction.h"
#include "../safety/transaction/TransactionValidator.h"
#include "../profile/UserProfile.h"
#include "../domain/Comparison.h"
#include<vector>

namespace polaris::explainability {

class ExplanationEngine {
public:
    // Explain a candidate/recommendation with profile and optional baseline
    static Explanation explainCandidate(
        const std::string& candidateId,
        const profile::UserProfile& profile,
        const domain::Recommendation* rec = nullptr,
        const domain::PerformanceBaseline* baseline = nullptr
    );

    // Explain a transaction lifecycle decision
    static Explanation explainTransaction(
        const safety::Transaction& tx,
        const profile::UserProfile& profile,
        const domain::Comparison* comparison = nullptr,
        const safety::ValidationResult* lastValidation = nullptr
    );

    // Explain comparison expected vs observed
    static Explanation explainComparison(
        const std::string& transactionId,
        const domain::Comparison& comparison,
        const std::string& expectedBenefit
    );

private:
    static std::string buildWhyNowCandidate(const std::string& candidateId, const profile::UserProfile& profile, const domain::Recommendation* rec, const domain::PerformanceBaseline* baseline);
    static std::string buildWhatWillChangeCandidate(const std::string& candidateId, const domain::Recommendation* rec);
    static std::string buildWhatWillNotChangeCandidate(const std::string& candidateId);
    static std::vector<std::string> buildRejectionConditionsCandidate(const std::string& candidateId, const profile::UserProfile& profile);
    
    static std::string buildWhyNowTransaction(const safety::Transaction& tx, const profile::UserProfile& profile);
    static std::string buildWhatWillChangeTransaction(const safety::Transaction& tx);
    static std::string buildWhatWillNotChangeTransaction(const safety::Transaction& tx);
    static std::vector<std::string> buildRejectionConditionsTransaction(const safety::Transaction& tx, const profile::UserProfile& profile, const safety::ValidationResult* lastValidation);
};

} // namespace polaris::explainability
