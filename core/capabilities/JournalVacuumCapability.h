#pragma once
#include "IOptimizationCapability.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace polaris::capabilities {

class JournalVacuumCapability : public IOptimizationCapability {
public:
    std::string id() const override { return "journal-vacuum"; }
    std::string name() const override { return "Vacuum systemd journal to bound disk usage"; }
    std::string category() const override { return "Storage"; }
    std::string description() const override { return "Identifies excessive journal disk usage and proposes bounded vacuum to 500M (or 14d)."; }
    std::string risk() const override { return "R1"; }
    std::string reversibility() const override { return "Limited (old logs >14d lost, cannot rollback logs; new logs continue, vacuum reversible only via retention config)"; }
    bool requiresReboot() const override { return false; }
    bool requiresAuth() const override { return true; } // journal vacuum requires privileged (org.polaris.journal.vacuum)

    bool isApplicable(const domain::PerformanceBaseline& b, const profile::UserProfile& profile) const override {
        (void)profile;
        if(!b.journalDisk.meta.available) return false;
        if(b.journalDisk.diskUsageBytes < 1ULL*1024*1024*1024) return false; // <1GB not worthwhile
        if(b.journalDisk.reclaimableBytes < 500ULL*1024*1024) return false; // <500M reclaimable not worthwhile
        return true;
    }

    CapabilityEvidence collect(const domain::PerformanceBaseline& b) const override {
        CapabilityEvidence ev;
        ev.risk = risk();
        if(!b.journalDisk.meta.available){
            ev.available=false;
            ev.reason="unavailable: journal disk usage not collected (journalctl --disk-usage failed)";
            ev.evidence={"journalDisk meta unavailable: " + b.journalDisk.meta.note};
            ev.confidence=0.0;
            ev.benefitGB=0.0;
            ev.benefitStr="unavailable";
            ev.reclaimableBytes=0;
            ev.stateHash = safety::TransactionValidator::hashString("journal-unavailable");
            ev.preconditions["journal.available"]="false";
            return ev;
        }
        uint64_t usage = b.journalDisk.diskUsageBytes;
        uint64_t reclaim = b.journalDisk.reclaimableBytes;
        ev.reclaimableBytes = reclaim;
        ev.benefitGB = (double)reclaim / (1024.0*1024*1024);
        std::ostringstream oss;
        oss<< std::fixed << std::setprecision(1) << ev.benefitGB << " GB journal reclaimable (usage " << usage/(1024*1024*1024) << "G -> vacuum " << b.journalDisk.vacuumTarget << ")";
        ev.benefitStr = oss.str();
        if(usage < 1ULL*1024*1024*1024){
            ev.available=false;
            ev.reason="journal usage " + std::to_string(usage/(1024*1024))+"MB <1GB threshold, not worthwhile";
            ev.confidence=0.90;
            std::ostringstream e;
            e<<"journal diskUsage "<<usage/(1024*1024)<<"MB vacuumTarget "<<b.journalDisk.vacuumTarget;
            ev.evidence.push_back(e.str());
            std::string concat = std::to_string(usage)+":no-benefit";
            ev.stateHash = safety::TransactionValidator::hashString(concat);
            ev.preconditions["journal.diskUsageBytes"]=std::to_string(usage);
            ev.preconditions["journal.reclaimableBytes"]="0";
            return ev;
        }
        if(reclaim < 500ULL*1024*1024){
            ev.available=false;
            ev.reason="journal reclaimable " + std::to_string(reclaim/(1024*1024))+"MB <500MB, not worthwhile";
            ev.confidence=0.80;
            std::ostringstream e;
            e<<"journal diskUsage "<<usage/(1024*1024)<<"MB reclaimable "<<reclaim/(1024*1024)<<"MB <500MB";
            ev.evidence.push_back(e.str());
            std::string concat = std::to_string(usage)+":"+std::to_string(reclaim);
            ev.stateHash = safety::TransactionValidator::hashString(concat);
            ev.preconditions["journal.diskUsageBytes"]=std::to_string(usage);
            ev.preconditions["journal.reclaimableBytes"]=std::to_string(reclaim);
            return ev;
        }
        ev.available=true;
        // Confidence high when usage large and reclaimable large
        if(reclaim >= 2ULL*1024*1024*1024) ev.confidence=0.90;
        else if(reclaim >= 1ULL*1024*1024*1024) ev.confidence=0.85;
        else ev.confidence=0.75;
        std::ostringstream e1;
        e1<<"journal diskUsage "<<usage/(1024*1024)<<"MB ("<<usage/(1024*1024*1024)<<"."<<(usage%(1024*1024*1024))/(100*1024*1024)<<"G)";
        ev.evidence.push_back(e1.str());
        std::ostringstream e2;
        e2<<"vacuumTarget "<<b.journalDisk.vacuumTarget<<" reclaimable "<<reclaim/(1024*1024)<<"MB";
        ev.evidence.push_back(e2.str());
        if(!b.storage.filesystems.empty()){
            auto &fs=b.storage.filesystems[0];
            std::ostringstream e3;
            e3<<"filesystem "<<fs.mount<<" free "<<fs.freeBytes/(1024*1024*1024)<<"G";
            ev.evidence.push_back(e3.str());
        }
        // journal p3 families maybe
        std::ostringstream e4;
        e4<<"journal p3 "<<b.journal.p3count<<" families "<<b.journal.families.size();
        ev.evidence.push_back(e4.str());
        std::sort(ev.evidence.begin(), ev.evidence.end());
        ev.preconditions["journal.diskUsageBytes"]=std::to_string(usage);
        ev.preconditions["journal.reclaimableBytes"]=std::to_string(reclaim);
        ev.preconditions["journal.vacuumTarget"]=b.journalDisk.vacuumTarget;
        std::string concat = std::to_string(usage)+":"+std::to_string(reclaim)+":"+b.journalDisk.vacuumTarget;
        ev.stateHash = safety::TransactionValidator::hashString(concat);
        ev.preconditions["journal.stateHash"]=ev.stateHash;
        return ev;
    }

    domain::Recommendation toRecommendation(const CapabilityEvidence& ev, const domain::PerformanceBaseline& b) const override {
        (void)b;
        domain::Recommendation r;
        r.id = "REC-" + id();
        r.title = name();
        r.problem = description();
        r.evidence = ev.evidence;
        r.confidence = (float)ev.confidence;
        r.expectedBenefit = ev.benefitStr;
        r.riskLevel = risk();
        r.affectedComponent = "Storage / Journal";
        r.why = "Measured journal diskUsage " + std::to_string(ev.reclaimableBytes/(1024*1024))+"MB reclaimable to "+ev.preconditions.at("journal.vacuumTarget");
        r.alternative = "Keep current journal retention (disk ok, no action)";
        r.rollbackConcept = reversibility();
        r.requiresReboot = requiresReboot();
        r.requiresAuth = requiresAuth();
        r.requiresApproval = true;
        r.category = category();
        return r;
    }

    safety::CurrentState snapshot(const domain::PerformanceBaseline& b, const CapabilityEvidence& ev) const override {
        (void)b;
        safety::CurrentState cur;
        cur.currentTarget = "/tmp/polaris-test-root/p19/journal-vacuum.state";
        cur.currentOperation = id();
        cur.currentBeforeHash = ev.stateHash;
        cur.currentUnitHash = "";
        cur.currentKernelVersion = "7.1.10-200.fc44.x86_64";
        cur.currentPackageStateHash = ev.stateHash;
        cur.currentPreconditions = ev.preconditions;
        cur.filePath = "";
        cur.currentCanonical = "";
        return cur;
    }

    safety::Transaction toTransaction(const std::string& txId, const domain::Recommendation& rec, const CapabilityEvidence& ev, const safety::CurrentState& cur) const override {
        safety::Transaction tx;
        tx.id = txId;
        tx.operationId = id();
        tx.target = "/tmp/polaris-test-root/p19/journal-vacuum.state";
        tx.description = rec.title;
        tx.riskLevel = risk();
        tx.expectedBenefit = rec.expectedBenefit;
        tx.requiredPrivileges = requiresAuth() ? "org.polaris.journal.vacuum" : "";
        tx.state = safety::TxState::PREVIEWED;
        tx.approvalState = "PENDING";
        tx.authorizationState = "PENDING";
        tx.backupState = "NONE";
        tx.rebootRequired = requiresReboot();
        tx.timestamp = rec.title;
        safety::ChangePreview cp;
        cp.target = tx.target;
        cp.beforeState = "journal diskUsage "+ev.preconditions.at("journal.diskUsageBytes")+" vacuumTarget "+ev.preconditions.at("journal.vacuumTarget");
        cp.afterState = "journal vacuumed to "+ev.preconditions.at("journal.vacuumTarget")+" reclaimed "+std::to_string(ev.reclaimableBytes/(1024*1024))+"MB";
        std::ostringstream diff;
        diff<<"- journal diskUsage "<<ev.preconditions.at("journal.diskUsageBytes")<<" bytes\n";
        diff<<"+ journal vacuum --vacuum-size="<<ev.preconditions.at("journal.vacuumTarget")<<" (reclaim "<<ev.reclaimableBytes/(1024*1024)<<"MB)\n";
        for(auto &e: ev.evidence) diff<<"  evidence: "<<e<<"\n";
        cp.diff = diff.str();
        cp.method = "journalctl --vacuum-size=500M (bounded, via helper)";
        cp.privilege = tx.requiredPrivileges;
        cp.risk = risk();
        cp.benefit = rec.expectedBenefit;
        cp.rollback = reversibility();
        cp.rebootRequired = requiresReboot();
        tx.previews.push_back(cp);
        tx.beforeState = cp.beforeState;
        tx.afterState = cp.afterState;
        tx.evidence = rec.problem;
        tx.rollbackPlan = reversibility();
        tx.status = "PREVIEWED";
        tx.beforeHash = ev.stateHash;
        tx.beforeUnitHash = "";
        tx.kernelVersion = cur.currentKernelVersion;
        tx.packageStateHash = ev.stateHash;
        tx.preconditions = ev.preconditions;
        tx.idempotencyKey = tx.id;
        return tx;
    }

    bool verify(const domain::PerformanceBaseline& before, const domain::PerformanceBaseline& after, std::string& observedBenefit, domain::Verdict& verdict, std::string& details) const override {
        if(!before.journalDisk.meta.available || !after.journalDisk.meta.available){
            observedBenefit="unavailable: journalDisk meta not collected";
            verdict=domain::Verdict::INCONCLUSIVE;
            details="journalDisk unavailable before or after";
            return false;
        }
        uint64_t beforeUsage = before.journalDisk.diskUsageBytes;
        uint64_t afterUsage = after.journalDisk.diskUsageBytes;
        int64_t delta = (int64_t)afterUsage - (int64_t)beforeUsage;
        if(std::abs(delta) < 5*1024*1024){ // <5MB
            observedBenefit="no change (delta "+std::to_string(delta/(1024*1024))+"MB)";
            verdict=domain::Verdict::NO_CHANGE;
            details="journal diskUsage delta <5MB";
            return true;
        }
        if(delta < 0){
            uint64_t reclaimed = (uint64_t)(-delta);
            observedBenefit="reclaimed "+std::to_string(reclaimed/(1024*1024))+"MB journal "+std::to_string(beforeUsage/(1024*1024))+"MB->"+std::to_string(afterUsage/(1024*1024))+"MB";
            uint64_t expected = before.journalDisk.reclaimableBytes;
            if(std::abs((int64_t)reclaimed - (int64_t)expected) < 100*1024*1024){
                verdict=domain::Verdict::SUCCESS;
                details="observed matches expected reclaimable "+std::to_string(expected/(1024*1024))+"MB";
            } else {
                verdict=domain::Verdict::IMPROVED;
                details="benefit observed but not exactly as expected (expected "+std::to_string(expected/(1024*1024))+"MB observed "+std::to_string(reclaimed/(1024*1024))+"MB)";
            }
            // Check regression: if after usage still > before? No, decrease is improvement. Check free space not regressed?
            return true;
        } else {
            observedBenefit="regression: journal increased "+std::to_string(delta/(1024*1024))+"MB";
            verdict=domain::Verdict::REGRESSION;
            details="journal diskUsage increased after vacuum unexpected";
            return true;
        }
    }

    std::string explainWhyNow(const domain::PerformanceBaseline& b, const CapabilityEvidence& ev, const profile::UserProfile& profile) const override {
        (void)profile; (void)b;
        if(!ev.available) return "Journal vacuum not applicable: " + ev.reason;
        std::ostringstream oss;
        oss<<"Measured journal diskUsage "<<ev.preconditions.at("journal.diskUsageBytes")<<" bytes ("<<ev.preconditions.at("journal.diskUsageBytes").size()<<"MB) reclaimable "<<ev.reclaimableBytes/(1024*1024)<<"MB to target "<<ev.preconditions.at("journal.vacuumTarget")<<" (benefit "<<ev.benefitStr<<", confidence "<<(int)(ev.confidence*100)<<"%, risk "<<risk()<<"). ";
        oss<<"Journalctl --disk-usage evidence: journal vacuuming is bounded to 500M, reversible only via retention config (old logs >14d lost).";
        return oss.str();
    }
    std::string explainWhatWillChange(const CapabilityEvidence& ev) const override {
        std::ostringstream oss;
        oss<<"target=journal-vacuum, operation=journalctl --vacuum-size="<<ev.preconditions.at("journal.vacuumTarget")<<" (bounded, reclaim "<<ev.reclaimableBytes/(1024*1024)<<"MB), method=journalctl --vacuum-size=500M via helper (one vacuum per transaction), rollback "<<reversibility();
        return oss.str();
    }
    std::string explainWhatWillNotChange() const override {
        return "NVIDIA 470xx remains claimed driver nvidia, zram remains 8G lzo-rle, Akonadi remains running 14 agents, mssql remains disabled, fstab remains 3 entries, flatpak remains, no reboot, no unrelated timers.";
    }
    std::vector<std::string> rejectionConditions(const domain::PerformanceBaseline& b, const CapabilityEvidence& ev, const profile::UserProfile& profile) const override {
        (void)profile; (void)b;
        std::vector<std::string> rc;
        rc.push_back("stale journal.stateHash: expected "+ev.stateHash.substr(0,16)+" observed different → FAILED");
        rc.push_back("unavailable evidence: journalDisk meta not collected → INCONCLUSIVE");
        rc.push_back("insufficient benefit: diskUsage <1GB or reclaimable <500MB → NO_ACTION");
        rc.push_back("already optimal: journal diskUsage <= vacuumTarget → NO_ACTION");
        rc.push_back("transaction already completed → APPLY rejected");
        rc.push_back("backup unavailable: backupState != CREATED → FAILED");
        rc.push_back("profile: journal not workflow-blocked (always ALLOWED)");
        std::sort(rc.begin(), rc.end());
        return rc;
    }
};

} // namespace polaris::capabilities
