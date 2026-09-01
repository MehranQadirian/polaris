#include "../../core/explainability/ExplanationEngine.h"
#include "../../core/profile/UserProfile.h"
#include "../../core/domain/PerfModels.h"
#include <cassert>
#include <iostream>

using namespace polaris::explainability;
using namespace polaris::profile;
using namespace polaris::domain;

void test_why_now_generation(){
    UserProfile p;
    p.setField("usesKMail", TriState::UNKNOWN);
    auto exp = ExplanationEngine::explainCandidate("akonadi-disable", p, nullptr, nullptr);
    assert(!exp.whyNow.empty());
    assert(exp.whyNow.find("Akonadi")!=std::string::npos || exp.whyNow.find("akonadi")!=std::string::npos);
    assert(exp.whyNow.find("unknown")!=std::string::npos || exp.whyNow.find("1302M")!=std::string::npos);
    std::cout << "WHY NOW generation PASS: " << exp.whyNow.substr(0,60) << "...\n";
}

void test_what_will_change(){
    UserProfile p;
    Recommendation rec;
    rec.title = "Disable Akonadi";
    rec.affectedComponent = "akonadi";
    rec.requiresReboot = false;
    auto exp = ExplanationEngine::explainCandidate("akonadi-disable", p, &rec, nullptr);
    assert(!exp.whatWillChange.empty());
    assert(exp.whatWillChange.find("akonadi")!=std::string::npos || exp.whatWillChange.find("target")!=std::string::npos);
    std::cout << "WHAT WILL CHANGE PASS: " << exp.whatWillChange.substr(0,60) << "...\n";
}

void test_what_will_not_change(){
    UserProfile p;
    auto exp = ExplanationEngine::explainCandidate("akonadi-disable", p, nullptr, nullptr);
    assert(!exp.whatWillNotChange.empty());
    assert(exp.whatWillNotChange.find("NVIDIA")!=std::string::npos);
    assert(exp.whatWillNotChange.find("fstab")!=std::string::npos || exp.whatWillNotChange.find("remains")!=std::string::npos);
    // Scope-aware: fstab candidate should not say fstab remains unchanged
    auto exp2 = ExplanationEngine::explainCandidate("fstab-stale-swap", p, nullptr, nullptr);
    assert(exp2.whatWillNotChange.find("Akonadi")!=std::string::npos);
    std::cout << "WHAT WILL NOT CHANGE PASS\n";
}

void test_rejection_explanation_stale(){
    UserProfile p;
    // For candidate, rejectionConditions should include stale beforeHash
    auto exp = ExplanationEngine::explainCandidate("fstab-stale-swap", p, nullptr, nullptr);
    bool foundStale=false;
    for(auto &rc: exp.rejectionConditions) if(rc.find("stale beforeHash")!=std::string::npos) foundStale=true;
    assert(foundStale);
    std::cout << "rejection explanation (stale) PASS\n";
}

void test_profile_blocked_explanation(){
    UserProfile p;
    p.setField("usesKMail", TriState::YES);
    auto exp = ExplanationEngine::explainCandidate("akonadi-disable", p, nullptr, nullptr);
    assert(exp.decision==DecisionKind::BLOCKED);
    assert(exp.decisionLabel=="BLOCKED_BY_USER_WORKFLOW");
    assert(exp.rejectionConditions.size()>=1);
    bool found=false;
    for(auto &rc: exp.rejectionConditions) if(rc.find("usesKMail=yes")!=std::string::npos) found=true;
    assert(found);
    assert(exp.whatWillNotChange.find("Akonadi will remain")!=std::string::npos);
    std::cout << "profile-blocked explanation PASS\n";
}

void test_unknown_profile_explanation(){
    UserProfile p; // all unknown
    auto exp = ExplanationEngine::explainCandidate("akonadi-disable", p, nullptr, nullptr);
    assert(exp.decision==DecisionKind::REQUIRE_CONFIRMATION);
    assert(exp.decisionLabel=="REQUIRES_USER_CONFIRMATION");
    assert(exp.whyNow.find("unknown")!=std::string::npos);
    std::cout << "unknown profile explanation PASS\n";
}

void test_profile_allowed_explanation(){
    UserProfile p;
    p.setField("usesKMail", TriState::NO);
    p.setField("usesKontact", TriState::NO);
    p.setField("usesKOrganizer", TriState::NO);
    p.setField("usesAkonadi", TriState::NO);
    auto exp = ExplanationEngine::explainCandidate("akonadi-disable", p, nullptr, nullptr);
    assert(exp.decision==DecisionKind::RECOMMEND);
    assert(exp.decisionLabel=="RECOMMEND" || exp.decisionLabel=="REQUIRE_CONFIRMATION" || exp.decisionLabel=="BLOCKED" );
    // For all NO, should be RECOMMEND (allowed for analysis) not BLOCKED
    assert(exp.decision!=DecisionKind::BLOCKED);
    std::cout << "profile allowed explanation PASS (decision " << toString(exp.decision) << ")\n";
}

int main(){
    test_why_now_generation();
    test_what_will_change();
    test_what_will_not_change();
    test_rejection_explanation_stale();
    test_profile_blocked_explanation();
    test_unknown_profile_explanation();
    test_profile_allowed_explanation();
    std::cout << "All P16 explain candidate tests PASS\n";
    return 0;
}
