#pragma once
#include "UserProfile.h"
#include <string>

namespace polaris::profile {

enum class Decision {
    BLOCKED_BY_USER_WORKFLOW,
    REQUIRES_USER_CONFIRMATION,
    ALLOWED_FOR_ANALYSIS
};

inline std::string toString(Decision d){
    switch(d){
        case Decision::BLOCKED_BY_USER_WORKFLOW: return "BLOCKED_BY_USER_WORKFLOW";
        case Decision::REQUIRES_USER_CONFIRMATION: return "REQUIRES_USER_CONFIRMATION";
        case Decision::ALLOWED_FOR_ANALYSIS: return "ALLOWED_FOR_ANALYSIS";
    }
    return "UNKNOWN";
}

struct AdvisorResult {
    Decision decision = Decision::REQUIRES_USER_CONFIRMATION;
    std::string reason;
    std::string causingField;
    std::string causingValue; // "yes"/"no"/"unknown"
    bool explicitFact = false; // true if YES/NO explicit, false if UNKNOWN
    std::string whatWillNotChange;
    std::string confirmationRequired;
    std::string candidate; // e.g., "akonadi"
};

class ProfileAdvisor {
public:
    static AdvisorResult canConsiderAkonadi(const UserProfile& profile);
    static AdvisorResult canConsiderBluetooth(const UserProfile& profile);
    static AdvisorResult canConsiderPrinting(const UserProfile& profile);
    static AdvisorResult canConsiderAvahi(const UserProfile& profile);
    static AdvisorResult canConsiderCups(const UserProfile& profile);
    static AdvisorResult canConsider(const UserProfile& profile, const std::string& candidateId);
};

} // namespace polaris::profile
