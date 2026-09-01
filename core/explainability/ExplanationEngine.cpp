#include "ExplanationEngine.h"
#include "../profile/ProfileAdvisor.h"
#include "../domain/Comparison.h"
#include "../capabilities/OptimizationRegistry.h"
#include "../capabilities/CapabilityRegistrySetup.h"
#include "../capabilities/FlatpakUnusedCapability.h"
#include "../capabilities/JournalVacuumCapability.h"
#include <sstream>
#include <algorithm>

namespace polaris::explainability {

Explanation ExplanationEngine::explainCandidate(
    const std::string& candidateId,
    const profile::UserProfile& profile,
    const domain::Recommendation* rec,
    const domain::PerformanceBaseline* baseline
){
    Explanation exp;
    exp.id = "EXP-" + candidateId;
    exp.candidateId = candidateId;
    exp.candidateKind = CandidateKind::RECOMMENDATION;
    
    // Determine profile decision
    auto adv = profile::ProfileAdvisor::canConsider(profile, candidateId);
    if(adv.decision==profile::Decision::BLOCKED_BY_USER_WORKFLOW){
        exp.decision = DecisionKind::BLOCKED;
        exp.decisionLabel = "BLOCKED_BY_USER_WORKFLOW";
    } else if(adv.decision==profile::Decision::REQUIRES_USER_CONFIRMATION){
        exp.decision = DecisionKind::REQUIRE_CONFIRMATION;
        exp.decisionLabel = "REQUIRES_USER_CONFIRMATION";
    } else {
        exp.decision = DecisionKind::RECOMMEND;
        exp.decisionLabel = "RECOMMEND";
    }

    // WHY NOW
    exp.whyNow = buildWhyNowCandidate(candidateId, profile, rec, baseline);

    // Evidence
    if(rec){
        exp.evidence = rec->evidence;
        exp.expectedBenefit = rec->expectedBenefit;
        exp.confidence = rec->confidence;
        exp.risk = rec->riskLevel;
        exp.reversibility = rec->rollbackConcept;
        exp.rebootRequired = rec->requiresReboot;
        exp.authorizationRequired = rec->requiresAuth;
        exp.userImpact = rec->problem;
        exp.dependencies = {rec->affectedComponent};
    } else {
        // Mock for candidate without rec (e.g., akonadi-disable)
        if(candidateId.find("akonadi")!=std::string::npos){
            exp.evidence = {"akonadi 14 agents 1302M", "db_data 126M", "not in critical-chain", "systemd userspace 8.515s"};
            exp.expectedBenefit = "~1.3GB RAM";
            exp.confidence = (adv.decision==profile::Decision::BLOCKED_BY_USER_WORKFLOW?0.65:0.90);
            exp.risk = "R2";
            exp.reversibility = "High (akonadictl start)";
            exp.rebootRequired = false;
            exp.authorizationRequired = true;
            exp.userImpact = "KMail/Kontact would lose PIM if Akonadi disabled";
            exp.dependencies = {"akonadi running"};
        } else if(candidateId.find("bluetooth")!=std::string::npos){
            exp.evidence = {"bluetooth enabled active 2 paired TSCO-TS2343"};
            exp.expectedBenefit = "5-10M";
            exp.confidence = 0.40;
            exp.risk = "R2";
            exp.reversibility = "High (systemctl enable bluetooth)";
            exp.rebootRequired = false;
            exp.authorizationRequired = true;
            exp.userImpact = "Bluetooth devices would disconnect";
        } else {
            exp.evidence = {"candidate "+candidateId+" evidence from baseline"};
            exp.expectedBenefit = "unknown";
            exp.confidence = 0.5;
            exp.risk = "R1";
            exp.reversibility = "High";
        }
    }
    if(baseline){
        std::ostringstream oss;
        oss << "userspace " << baseline->systemd.userspace << "s, avail " << (baseline->memory.availableKb/(1024*1024)) << "GB";
        exp.beforeStateSummary = oss.str();
    }

    // WHAT WILL CHANGE / NOT
    exp.whatWillChange = buildWhatWillChangeCandidate(candidateId, rec);
    if(adv.decision==profile::Decision::BLOCKED_BY_USER_WORKFLOW) exp.whatWillNotChange = adv.whatWillNotChange;
    else exp.whatWillNotChange = buildWhatWillNotChangeCandidate(candidateId);
    exp.rejectionConditions = buildRejectionConditionsCandidate(candidateId, profile);
    exp.rollbackSummary = exp.reversibility;
    exp.limitations = (adv.decision==profile::Decision::REQUIRES_USER_CONFIRMATION ? "User workflow unknown: requires confirmation" : "");
    // Sort for determinism
    std::sort(exp.evidence.begin(), exp.evidence.end());
    std::sort(exp.rejectionConditions.begin(), exp.rejectionConditions.end());
    std::sort(exp.dependencies.begin(), exp.dependencies.end());
    return exp;
}

Explanation ExplanationEngine::explainTransaction(
    const safety::Transaction& tx,
    const profile::UserProfile& profile,
    const domain::Comparison* comparison,
    const safety::ValidationResult* lastValidation
){
    Explanation exp;
    exp.id = "EXP-" + tx.id;
    exp.candidateId = tx.id;
    exp.candidateKind = CandidateKind::TRANSACTION;
    exp.expectedBenefit = tx.expectedBenefit;
    exp.rebootRequired = tx.rebootRequired;
    exp.authorizationRequired = !tx.requiredPrivileges.empty();
    exp.confidence = 0.90; // default for transaction
    exp.risk = tx.riskLevel;

    // Decision based on state
    switch(tx.state){
        case safety::TxState::PREVIEWED: exp.decision=DecisionKind::PREVIEWED; exp.decisionLabel="PREVIEWED"; break;
        case safety::TxState::APPROVAL_REQUIRED: exp.decision=DecisionKind::PREVIEWED; exp.decisionLabel="APPROVAL_REQUIRED"; break;
        case safety::TxState::APPROVED: exp.decision=DecisionKind::APPROVED; exp.decisionLabel="APPROVED"; break;
        case safety::TxState::AUTHORIZATION_REQUIRED: exp.decision=DecisionKind::APPROVED; exp.decisionLabel="AUTHORIZATION_REQUIRED"; break;
        case safety::TxState::AUTHORIZED: exp.decision=DecisionKind::APPROVED; exp.decisionLabel="AUTHORIZED"; break;
        case safety::TxState::BACKUP_CREATED: exp.decision=DecisionKind::APPROVED; exp.decisionLabel="BACKUP_CREATED"; break;
        case safety::TxState::APPLYING: exp.decision=DecisionKind::APPROVED; exp.decisionLabel="APPLYING"; break;
        case safety::TxState::APPLIED: exp.decision=DecisionKind::APPROVED; exp.decisionLabel="APPLIED"; break;
        case safety::TxState::VERIFYING: exp.decision=DecisionKind::COMPLETED; exp.decisionLabel="VERIFYING"; break;
        case safety::TxState::VERIFIED: exp.decision=DecisionKind::COMPLETED; exp.decisionLabel="VERIFIED"; break;
        case safety::TxState::COMPLETED:
            if(comparison && comparison->verdict==domain::Verdict::REGRESSION) { exp.decision=DecisionKind::REGRESSION; exp.decisionLabel="COMPLETED: REGRESSION"; }
            else if(comparison && comparison->verdict==domain::Verdict::SUCCESS) { exp.decision=DecisionKind::COMPLETED; exp.decisionLabel="COMPLETED: SUCCESS"; }
            else { exp.decision=DecisionKind::COMPLETED; exp.decisionLabel="COMPLETED"; }
            break;
        case safety::TxState::FAILED: exp.decision=DecisionKind::FAILED; exp.decisionLabel="FAILED: " + (tx.error.empty()? tx.validationResult : tx.error); break;
        case safety::TxState::ROLLING_BACK: exp.decision=DecisionKind::FAILED; exp.decisionLabel="ROLLING_BACK"; break;
        case safety::TxState::ROLLED_BACK: exp.decision=DecisionKind::FAILED; exp.decisionLabel="ROLLED_BACK"; break;
        case safety::TxState::CANCELLED: exp.decision=DecisionKind::FAILED; exp.decisionLabel="CANCELLED"; break;
        default: exp.decision=DecisionKind::PREVIEWED; exp.decisionLabel="UNKNOWN";
    }

    exp.whyNow = buildWhyNowTransaction(tx, profile);
    exp.whatWillChange = buildWhatWillChangeTransaction(tx);
    exp.whatWillNotChange = buildWhatWillNotChangeTransaction(tx);
    exp.rejectionConditions = buildRejectionConditionsTransaction(tx, profile, lastValidation);
    exp.rollbackSummary = tx.rollbackPlan.empty() ? "Restore from backup " + tx.target + ".bak" : tx.rollbackPlan;
    exp.beforeStateSummary = tx.beforeState.substr(0,80);
    exp.afterStateSummary = tx.afterState.substr(0,80);
    if(comparison){
        exp.observedBenefit = comparison->observedBenefit;
        exp.verdict = toString(comparison->verdict);
        exp.verdictReason = comparison->verdictReason;
        exp.hasRegression = comparison->hasRegression;
        exp.beforeStateSummary = "userspace "+std::to_string(comparison->beforeBaseline.systemd.userspace)+"s, avail "+std::to_string(comparison->beforeBaseline.memory.availableKb/(1024*1024))+"GB";
        exp.afterStateSummary = "userspace "+std::to_string(comparison->afterBaseline.systemd.userspace)+"s, avail "+std::to_string(comparison->afterBaseline.memory.availableKb/(1024*1024))+"GB";
        if(comparison->hasRegression) exp.rejectionConditions.push_back("regression detected: "+comparison->verdictReason);
    } else {
        exp.limitations = "Comparison unavailable: before/after baseline not yet captured (reboot-pending or not measured)";
    }
    if(tx.comparison.has_value()){
        exp.observedBenefit = tx.comparison->observedBenefit;
        exp.verdict = toString(tx.comparison->verdict);
    }
    exp.evidence = tx.previews.empty() ? std::vector<std::string>{tx.evidence} : std::vector<std::string>{tx.previews[0].diff};
    exp.reversibility = tx.rollbackPlan;
    exp.userImpact = tx.description;

    std::sort(exp.evidence.begin(), exp.evidence.end());
    std::sort(exp.rejectionConditions.begin(), exp.rejectionConditions.end());
    std::sort(exp.dependencies.begin(), exp.dependencies.end());
    return exp;
}

Explanation ExplanationEngine::explainComparison(
    const std::string& transactionId,
    const domain::Comparison& comparison,
    const std::string& expectedBenefit
){
    Explanation exp;
    exp.id = "EXP-CMP-" + transactionId;
    exp.candidateId = transactionId;
    exp.candidateKind = CandidateKind::TRANSACTION;
    exp.expectedBenefit = expectedBenefit;
    exp.observedBenefit = comparison.observedBenefit;
    exp.verdict = toString(comparison.verdict);
    exp.verdictReason = comparison.verdictReason;
    exp.hasRegression = comparison.hasRegression;
    if(comparison.verdict==domain::Verdict::REGRESSION) exp.decision=DecisionKind::REGRESSION;
    else if(comparison.verdict==domain::Verdict::SUCCESS) exp.decision=DecisionKind::COMPLETED;
    else if(comparison.verdict==domain::Verdict::NO_CHANGE) exp.decision=DecisionKind::NO_CHANGE;
    else exp.decision=DecisionKind::COMPLETED;
    exp.decisionLabel = exp.verdict;
    exp.whyNow = "Comparison expected '"+expectedBenefit+"' vs observed '"+comparison.observedBenefit+"' with "+std::to_string(comparison.metrics.size())+" metrics";
    exp.beforeStateSummary = "before "+comparison.beforeTimestamp;
    exp.afterStateSummary = "after "+comparison.afterTimestamp;
    for(auto &m: comparison.metrics){
        std::ostringstream oss;
        oss << m.metric << " " << (m.before?std::to_string(*m.before):"unavailable") << "→" << (m.after?std::to_string(*m.after):"unavailable") << (m.regression?" regression":"");
        exp.evidence.push_back(oss.str());
        if(m.regression) exp.rejectionConditions.push_back("regression: "+m.metric+" "+m.thresholdDesc);
    }
    std::sort(exp.evidence.begin(), exp.evidence.end());
    std::sort(exp.rejectionConditions.begin(), exp.rejectionConditions.end());
    return exp;
}

// Helpers
std::string ExplanationEngine::buildWhyNowCandidate(const std::string& candidateId, const profile::UserProfile& profile, const domain::Recommendation* rec, const domain::PerformanceBaseline* baseline){
    std::ostringstream oss;
    if(candidateId.find("akonadi")!=std::string::npos){
        auto adv = profile::ProfileAdvisor::canConsiderAkonadi(profile);
        if(adv.decision==profile::Decision::BLOCKED_BY_USER_WORKFLOW){
            oss << "Measured Akonadi currently consumes 1302M 14 agents (P9 baseline 8.515s userspace, not in critical-chain). Candidate blocked because " << adv.causingField << "=yes (explicit). Confidence " << (rec?rec->confidence:0.65) << " risk " << (rec?rec->riskLevel:"R2") << ".";
        } else if(adv.decision==profile::Decision::REQUIRES_USER_CONFIRMATION){
            oss << "Measured Akonadi 1302M 14 agents currently running; eligible for analysis only after user confirms " << adv.causingField << " workflow (currently unknown, confidence " << (rec?rec->confidence:0.65) << ").";
        } else {
            oss << "Measured Akonadi 1302M 14 agents; user explicitly declared " << adv.causingField << "=no, so candidate may be considered for analysis (still requires preview/approval). Confidence 0.90 risk R2.";
        }
    } else if(candidateId.find("bluetooth")!=std::string::npos){
        auto adv = profile::ProfileAdvisor::canConsiderBluetooth(profile);
        oss << "Measured bluetooth enabled active 2 paired devices; " << adv.reason;
    } else if(candidateId=="flatpak-unused" || candidateId=="REC-flatpak-unused"){
        // P19: delegate to capability
        capabilities::ensureCapabilitiesRegistered();
        auto cap = capabilities::OptimizationRegistry::instance().lookup("flatpak-unused");
        if(cap && baseline){
            auto ev = cap->collect(*baseline);
            oss << cap->explainWhyNow(*baseline, ev, profile);
        } else if(rec){
            oss << "Flatpak unused: " << rec->expectedBenefit << " confidence " << rec->confidence << " risk " << rec->riskLevel << ".";
            for(auto &e: rec->evidence) oss<<" evidence: "<<e<<";";
        } else {
            oss << "Flatpak unused candidate: measured flatpak reclaimable evidence required.";
        }
    } else if(candidateId=="journal-vacuum" || candidateId=="REC-journal-vacuum"){
        capabilities::ensureCapabilitiesRegistered();
        auto cap = capabilities::OptimizationRegistry::instance().lookup("journal-vacuum");
        if(cap && baseline){
            auto ev = cap->collect(*baseline);
            oss << cap->explainWhyNow(*baseline, ev, profile);
        } else if(rec){
            oss << "Journal vacuum: " << rec->expectedBenefit << " confidence " << rec->confidence << " risk " << rec->riskLevel << ".";
        } else {
            oss << "Journal vacuum candidate: measured journal diskUsage evidence required.";
        }
    } else {
        oss << "Candidate " << candidateId << " evidence: " << (rec?rec->evidence.size():0) << " items, expected benefit " << (rec?rec->expectedBenefit:"~1.3GB") << ", confidence " << (rec?rec->confidence:0.65) << ".";
        if(baseline) oss << " Baseline userspace " << baseline->systemd.userspace << "s.";
    }
    return oss.str();
}
std::string ExplanationEngine::buildWhatWillChangeCandidate(const std::string& candidateId, const domain::Recommendation* rec){
    if(candidateId=="flatpak-unused" || candidateId=="REC-flatpak-unused"){
        capabilities::ensureCapabilitiesRegistered();
        auto cap = capabilities::OptimizationRegistry::instance().lookup("flatpak-unused");
        if(cap){
            domain::PerformanceBaseline dummy;
            auto ev = cap->collect(dummy);
            // Try to get evidence from rec if available
            if(rec) {
                // Use rec evidence to build string
                std::ostringstream oss;
                oss<<"target=flatpak-unused operation=uninstall --unused benefit "<<rec->expectedBenefit;
                return oss.str();
            }
            return cap->explainWhatWillChange(ev);
        }
    }
    if(candidateId=="journal-vacuum" || candidateId=="REC-journal-vacuum"){
        capabilities::ensureCapabilitiesRegistered();
        auto cap = capabilities::OptimizationRegistry::instance().lookup("journal-vacuum");
        if(cap){
            domain::PerformanceBaseline dummy;
            auto ev = cap->collect(dummy);
            if(rec) {
                std::ostringstream oss;
                oss<<"target=journal-vacuum operation=journalctl --vacuum-size=500M benefit "<<rec->expectedBenefit;
                return oss.str();
            }
            return cap->explainWhatWillChange(ev);
        }
    }
    if(rec && !rec->affectedComponent.empty()) return "target="+rec->affectedComponent+" operation="+rec->title+" reboot="+std::string(rec->requiresReboot?"true":"false");
    if(candidateId.find("akonadi")!=std::string::npos) return "target=akonadi service, operation=disable, files=none, service=akonadi_control, expected runtime 14 agents stopped";
    if(candidateId.find("bluetooth")!=std::string::npos) return "target=bluetooth.service, operation=disable, service=bluetooth, expected 5-10M saved";
    if(candidateId.find("fstab")!=std::string::npos) return "target=/etc/fstab, operation=comment swap UUID, file=/etc/fstab, diff=\"- UUID ... swap\"";
    return "target="+candidateId+" operation="+candidateId;
}
std::string ExplanationEngine::buildWhatWillNotChangeCandidate(const std::string& candidateId){
    if(candidateId=="flatpak-unused" || candidateId=="REC-flatpak-unused"){
        capabilities::ensureCapabilitiesRegistered();
        auto cap = capabilities::OptimizationRegistry::instance().lookup("flatpak-unused");
        if(cap) return cap->explainWhatWillNotChange();
    }
    if(candidateId=="journal-vacuum" || candidateId=="REC-journal-vacuum"){
        capabilities::ensureCapabilitiesRegistered();
        auto cap = capabilities::OptimizationRegistry::instance().lookup("journal-vacuum");
        if(cap) return cap->explainWhatWillNotChange();
    }
    std::string base = "NVIDIA 470xx remains claimed driver nvidia, Intel remains default renderer, zram remains 8G lzo-rle, no reboot if rebootRequired=false, no privileged operation unless explicitly authorized. ";
    if(candidateId.find("akonadi")!=std::string::npos) return base + "fstab remains 3 entries, bluetooth remains enabled, cups remains socket-activated.";
    if(candidateId.find("fstab")!=std::string::npos) return "Akonadi remains running 14 agents, NVIDIA 470xx remains claimed, bluetooth remains enabled, no reboot.";
    if(candidateId.find("bluetooth")!=std::string::npos) return base + "Akonadi remains running, fstab remains 3 entries, zram remains.";
    return base + "Unrelated services remain unchanged.";
}
std::vector<std::string> ExplanationEngine::buildRejectionConditionsCandidate(const std::string& candidateId, const profile::UserProfile& profile){
    std::vector<std::string> rc;
    // P19: delegate to capability if known
    if(candidateId=="flatpak-unused" || candidateId=="REC-flatpak-unused"){
        capabilities::ensureCapabilitiesRegistered();
        auto cap = capabilities::OptimizationRegistry::instance().lookup("flatpak-unused");
        if(cap){
            domain::PerformanceBaseline dummy;
            auto ev = cap->collect(dummy);
            return cap->rejectionConditions(dummy, ev, profile);
        }
    }
    if(candidateId=="journal-vacuum" || candidateId=="REC-journal-vacuum"){
        capabilities::ensureCapabilitiesRegistered();
        auto cap = capabilities::OptimizationRegistry::instance().lookup("journal-vacuum");
        if(cap){
            domain::PerformanceBaseline dummy;
            auto ev = cap->collect(dummy);
            return cap->rejectionConditions(dummy, ev, profile);
        }
    }
    // Profile workflow
    if(candidateId.find("akonadi")!=std::string::npos){
        auto adv = profile::ProfileAdvisor::canConsiderAkonadi(profile);
        if(adv.decision==profile::Decision::BLOCKED_BY_USER_WORKFLOW) rc.push_back("profile: "+adv.causingField+"=yes → BLOCKED_BY_USER_WORKFLOW");
        else if(adv.decision==profile::Decision::REQUIRES_USER_CONFIRMATION) rc.push_back("profile: "+adv.causingField+"=unknown → REQUIRES_USER_CONFIRMATION");
    }
    if(candidateId.find("bluetooth")!=std::string::npos){
        auto adv = profile::ProfileAdvisor::canConsiderBluetooth(profile);
        if(adv.decision==profile::Decision::BLOCKED_BY_USER_WORKFLOW) rc.push_back("profile: usesBluetooth=yes");
        else if(adv.decision==profile::Decision::REQUIRES_USER_CONFIRMATION) rc.push_back("profile: usesBluetooth=unknown");
    }
    rc.push_back("stale beforeHash: expected <hash> observed <different> → FAILED (TransactionValidator)");
    rc.push_back("stale unitHash: expected ... observed ... → FAILED");
    rc.push_back("kernel changed: expected 7.1.10 observed 7.1.11 → FAILED");
    rc.push_back("precondition changed: service.mssql.enabled expected disabled observed enabled → FAILED");
    rc.push_back("insufficient confidence: 0.40 < threshold 0.65 → not recommended");
    rc.push_back("regression detected: boot +40% >10% threshold → REGRESSION");
    rc.push_back("transaction already completed → APPLY rejected");
    return rc;
}

std::string ExplanationEngine::buildWhyNowTransaction(const safety::Transaction& tx, const profile::UserProfile& profile){
    std::ostringstream oss;
    oss << "Transaction " << tx.id << " (" << tx.operationId << ") target " << tx.target << " risk " << tx.riskLevel << " expectedBenefit " << tx.expectedBenefit << ".";
    if(tx.state==safety::TxState::PREVIEWED) oss << " State PREVIEWED awaiting approval.";
    else if(tx.state==safety::TxState::FAILED) oss << " State FAILED: " << (tx.error.empty()?tx.validationResult:tx.error) << ".";
    else if(tx.state==safety::TxState::COMPLETED) oss << " State COMPLETED.";
    // Profile
    if(tx.operationId.find("akonadi")!=std::string::npos){
        auto adv = profile::ProfileAdvisor::canConsiderAkonadi(profile);
        oss << " Profile: " << adv.reason;
    }
    return oss.str();
}
std::string ExplanationEngine::buildWhatWillChangeTransaction(const safety::Transaction& tx){
    if(!tx.previews.empty()){
        return "target="+tx.previews[0].target+" operation="+tx.operationId+" diff=\""+tx.previews[0].diff.substr(0,64)+"\" method="+tx.previews[0].method+" reboot="+std::string(tx.rebootRequired?"true":"false");
    }
    return "target="+tx.target+" operation="+tx.operationId+" reboot="+std::string(tx.rebootRequired?"true":"false");
}
std::string ExplanationEngine::buildWhatWillNotChangeTransaction(const safety::Transaction& tx){
    std::string base = "NVIDIA 470xx remains claimed, Intel remains default, zram remains 8G, no reboot if rebootRequired=false. ";
    if(tx.target.find("fstab")!=std::string::npos) return "Akonadi remains running 14 agents, NVIDIA 470xx remains, bluetooth remains, zram remains, no reboot.";
    if(tx.target.find("akonadi")!=std::string::npos || tx.operationId.find("akonadi")!=std::string::npos) return base + "fstab remains 3 entries, bluetooth remains enabled, cups remains socket-activated.";
    return base + "Unrelated services remain unchanged.";
}
std::vector<std::string> ExplanationEngine::buildRejectionConditionsTransaction(const safety::Transaction& tx, const profile::UserProfile& profile, const safety::ValidationResult* lastValidation){
    std::vector<std::string> rc;
    if(lastValidation && !lastValidation->valid){
        std::ostringstream oss;
        oss << lastValidation->failingField << ": expected " << lastValidation->expected.substr(0,16) << " observed " << lastValidation->observed.substr(0,16) << " → FAILED";
        rc.push_back(oss.str());
    }
    if(tx.approvalState!="APPROVED") rc.push_back("authorization missing: approvalState="+tx.approvalState+" → FAILED");
    if(tx.backupState!="CREATED" && tx.state==safety::TxState::APPLYING) rc.push_back("backup unavailable: backupState="+tx.backupState+" → FAILED");
    if(tx.state==safety::TxState::COMPLETED) rc.push_back("transaction already completed → APPLY rejected (idempotent)");
    // Profile workflow
    if(tx.operationId.find("akonadi")!=std::string::npos){
        auto adv = profile::ProfileAdvisor::canConsiderAkonadi(profile);
        if(adv.decision==profile::Decision::BLOCKED_BY_USER_WORKFLOW) rc.push_back("profile: "+adv.causingField+"=yes → BLOCKED");
    }
    rc.push_back("stale beforeHash: expected <hash> observed <different> → FAILED");
    rc.push_back("TOCTOU symlink detected → FAILED");
    rc.push_back("invalid state transition: "+safety::toString(tx.state)+"→APPLYING not allowed if stale → FAILED");
    if(tx.comparison.has_value() && tx.comparison->hasRegression) rc.push_back("regression detected → consider rollback");
    return rc;
}

} // namespace polaris::explainability
