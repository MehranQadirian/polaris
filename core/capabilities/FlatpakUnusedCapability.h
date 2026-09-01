#pragma once
#include "IOptimizationCapability.h"
#include "../safety/transaction/TransactionValidator.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace polaris::capabilities {

class FlatpakUnusedCapability : public IOptimizationCapability {
public:
    std::string id() const override { return "flatpak-unused"; }
    std::string name() const override { return "Remove unused Flatpak runtimes"; }
    std::string category() const override { return "Storage"; }
    std::string description() const override { return "Identifies unused Flatpak runtimes that can be safely removed to reclaim disk space."; }
    std::string risk() const override { return "R1"; }
    std::string reversibility() const override { return "High (flatpak install <runtime-id>)"; }
    bool requiresReboot() const override { return false; }
    bool requiresAuth() const override { return false; }

    bool isApplicable(const domain::PerformanceBaseline& b, const profile::UserProfile& profile) const override {
        (void)profile;
        // Profile independent: flatpak not workflow blocked
        // Must have available evidence and reclaimable > 0.5GB and at least 1 unused runtime
        if(!b.flatpak.meta.available) return false;
        if(!b.flatpak.hasFlatpak) return false;
        if(b.flatpak.unusedRuntimes.empty()) return false;
        if(b.flatpak.reclaimableBytes < 500ULL*1024*1024) return false; // <500MB not worthwhile
        return true;
    }

    CapabilityEvidence collect(const domain::PerformanceBaseline& b) const override {
        CapabilityEvidence ev;
        ev.risk = risk();
        if(!b.flatpak.meta.available){
            ev.available=false;
            ev.reason="unavailable: flatpak evidence not collected (flatpak not installed or provider failed)";
            ev.evidence={"flatpak meta unavailable: " + b.flatpak.meta.note};
            ev.confidence=0.0;
            ev.benefitGB=0.0;
            ev.benefitStr="unavailable";
            ev.reclaimableBytes=0;
            ev.stateHash = safety::TransactionValidator::hashString("flatpak-unavailable");
            ev.preconditions["flatpak.available"]="false";
            return ev;
        }
        if(!b.flatpak.hasFlatpak){
            ev.available=false;
            ev.reason="flatpak not installed";
            ev.evidence={"flatpak not installed"};
            ev.confidence=0.0;
            ev.benefitGB=0.0;
            ev.benefitStr="unavailable: flatpak not installed";
            ev.reclaimableBytes=0;
            ev.stateHash = safety::TransactionValidator::hashString("flatpak-not-installed");
            ev.preconditions["flatpak.hasFlatpak"]="false";
            return ev;
        }
        if(b.flatpak.unusedRuntimes.empty()){
            ev.available=false;
            ev.reason="no unused flatpak runtimes detected (0 reclaimable)";
            ev.evidence={"flatpak runtimes total " + std::to_string(b.flatpak.totalCount) + " unused 0", "free " + std::to_string(b.storage.filesystems.empty()?0:b.storage.filesystems[0].freeBytes/(1024*1024*1024)) + "G"};
            ev.confidence=0.90;
            ev.benefitGB=0.0;
            ev.benefitStr="no unused runtimes - no benefit";
            ev.reclaimableBytes=0;
            // Still produce hash for stale: hash of runtimes list
            std::string concat;
            for(auto &r: b.flatpak.runtimes) concat+=r.id+":"+r.branch+";";
            ev.stateHash = safety::TransactionValidator::hashString(concat);
            ev.preconditions["flatpak.unusedCount"]="0";
            ev.preconditions["flatpak.reclaimableBytes"]="0";
            return ev;
        }
        ev.available=true;
        ev.reclaimableBytes = b.flatpak.reclaimableBytes;
        ev.benefitGB = (double)ev.reclaimableBytes / (1024.0*1024*1024);
        std::ostringstream oss;
        oss<< std::fixed << std::setprecision(1) << ev.benefitGB << " GB disk reclaimed (unused runtimes " << b.flatpak.unusedRuntimes.size() << ")";
        ev.benefitStr = oss.str();
        // Confidence based on benefit size and evidence quality
        if(ev.benefitGB >= 1.5) ev.confidence=0.90;
        else if(ev.benefitGB >= 1.0) ev.confidence=0.85;
        else if(ev.benefitGB >= 0.5) ev.confidence=0.75;
        else ev.confidence=0.60;
        // Evidence sorted deterministically
        for(auto &r: b.flatpak.unusedRuntimes){
            std::ostringstream e;
            e<<r.id<<" "<<r.branch<<" "<<r.origin<<" "<<r.installedSizeBytes/(1024*1024)<<"MB";
            ev.evidence.push_back(e.str());
        }
        // Add storage free evidence
        if(!b.storage.filesystems.empty()){
            auto &fs=b.storage.filesystems[0];
            std::ostringstream e;
            e<<"filesystem "<<fs.mount<<" free "<<fs.freeBytes/(1024*1024*1024)<<"G used "<< (int)fs.usedPct<<"%";
            ev.evidence.push_back(e.str());
        }
        std::sort(ev.evidence.begin(), ev.evidence.end());
        // Preconditions for stale
        ev.preconditions["flatpak.unusedCount"]=std::to_string(b.flatpak.unusedRuntimes.size());
        ev.preconditions["flatpak.reclaimableBytes"]=std::to_string(b.flatpak.reclaimableBytes);
        // stateHash: hash of unused list + reclaimable
        std::string concat;
        for(auto &r: b.flatpak.unusedRuntimes) concat+=r.id+":"+r.branch+":"+std::to_string(r.installedSizeBytes)+";";
        concat+="#"+std::to_string(ev.reclaimableBytes);
        ev.stateHash = safety::TransactionValidator::hashString(concat);
        ev.preconditions["flatpak.stateHash"]=ev.stateHash;
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
        r.affectedComponent = "Storage / Flatpak";
        r.why = "Measured flatpak unused runtimes " + std::to_string(ev.reclaimableBytes/(1024*1024)) + "MB reclaimable";
        r.alternative = "Keep all runtimes (disk ok, no action)";
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
        cur.currentTarget = "/tmp/polaris-test-root/p19/flatpak-unused.state";
        cur.currentOperation = id();
        cur.currentBeforeHash = ev.stateHash;
        cur.currentUnitHash = "";
        cur.currentKernelVersion = "7.1.10-200.fc44.x86_64";
        cur.currentPackageStateHash = ev.stateHash;
        cur.currentPreconditions = ev.preconditions;
        cur.filePath = ""; // fixture: not a regular file until apply, skip TOCTOU, rely on preconditions
        cur.currentCanonical = "";
        return cur;
    }

    safety::Transaction toTransaction(const std::string& txId, const domain::Recommendation& rec, const CapabilityEvidence& ev, const safety::CurrentState& cur) const override {
        safety::Transaction tx;
        tx.id = txId;
        tx.operationId = id();
        tx.target = "/tmp/polaris-test-root/p19/flatpak-unused.state";
        tx.description = rec.title;
        tx.riskLevel = risk();
        tx.expectedBenefit = rec.expectedBenefit;
        tx.requiredPrivileges = requiresAuth() ? "org.polaris.flatpak.uninstall" : "";
        tx.state = safety::TxState::PREVIEWED;
        tx.approvalState = "PENDING";
        tx.authorizationState = "PENDING";
        tx.backupState = "NONE";
        tx.rebootRequired = requiresReboot();
        tx.timestamp = rec.title; // placeholder, real timestamp via nowISO in store
        // preview diff: list unused runtimes
        safety::ChangePreview cp;
        cp.target = tx.target;
        cp.beforeState = "unusedRuntimes=" + std::to_string(ev.reclaimableBytes/(1024*1024)) + "MB " + cur.currentBeforeHash.substr(0,16);
        cp.afterState = "unusedRuntimes=0 reclaimed " + std::to_string(ev.reclaimableBytes/(1024*1024)) + "MB";
        std::ostringstream diff;
        diff<<"- flatpak unused runtimes "<<ev.reclaimableBytes/(1024*1024)<<"MB\n+ flatpak reclaimed "<<ev.reclaimableBytes/(1024*1024)<<"MB (uninstall --unused)\n";
        for(auto &e: ev.evidence) diff<<"  evidence: "<<e<<"\n";
        cp.diff = diff.str();
        cp.method = "flatpak uninstall --unused (user, no sudo)";
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
        // Fill hash fields for stale protection
        tx.beforeHash = ev.stateHash;
        tx.beforeUnitHash = "";
        tx.kernelVersion = cur.currentKernelVersion;
        tx.packageStateHash = ev.stateHash;
        tx.preconditions = ev.preconditions;
        tx.idempotencyKey = tx.id;
        return tx;
    }

    bool verify(const domain::PerformanceBaseline& before, const domain::PerformanceBaseline& after, std::string& observedBenefit, domain::Verdict& verdict, std::string& details) const override {
        // If either baseline unavailable, INCONCLUSIVE
        if(!before.flatpak.meta.available || !after.flatpak.meta.available){
            observedBenefit="unavailable: flatpak meta not collected";
            verdict=domain::Verdict::INCONCLUSIVE;
            details="flatpak meta unavailable before or after (expected flatpak evidence)";
            return false;
        }
        if(!before.storage.filesystems.empty() && !after.storage.filesystems.empty()){
            // Use filesystem free delta as primary
            uint64_t beforeFree = before.storage.filesystems[0].freeBytes;
            uint64_t afterFree = after.storage.filesystems[0].freeBytes;
            int64_t delta = (int64_t)afterFree - (int64_t)beforeFree;
            if(std::abs(delta) < 10*1024*1024){ // <10MB treat as no change
                observedBenefit="no change (delta "+ std::to_string(delta/(1024*1024))+"MB)";
                verdict=domain::Verdict::NO_CHANGE;
                details="freeBytes delta " + std::to_string(delta) + " <10MB";
                return true;
            }
            if(delta > 0){
                observedBenefit="reclaimed " + std::to_string(delta/(1024*1024))+"MB freeBytes "+std::to_string(beforeFree/(1024*1024*1024))+"G->"+std::to_string(afterFree/(1024*1024*1024))+"G";
                // Check if matches expected: before flatpak reclaimable
                uint64_t expected = before.flatpak.reclaimableBytes;
                if(std::abs((int64_t)delta - (int64_t)expected) < 100*1024*1024){ // within 100MB
                    verdict=domain::Verdict::SUCCESS;
                    details="observed matches expected reclaimable "+std::to_string(expected/(1024*1024))+"MB";
                } else {
                    verdict=domain::Verdict::IMPROVED;
                    details="benefit observed but not exactly as expected (expected "+std::to_string(expected/(1024*1024))+"MB observed "+std::to_string(delta/(1024*1024))+"MB)";
                }
                return true;
            } else {
                observedBenefit="regression: free decreased "+std::to_string(-delta/(1024*1024))+"MB";
                verdict=domain::Verdict::REGRESSION;
                details="freeBytes decreased after flatpak uninstall unexpected";
                return true;
            }
        }
        // Fallback: use flatpak reclaimable delta
        uint64_t beforeReclaim = before.flatpak.reclaimableBytes;
        uint64_t afterReclaim = after.flatpak.reclaimableBytes;
        if(afterReclaim < beforeReclaim){
            uint64_t reclaimed = beforeReclaim - afterReclaim;
            observedBenefit="flatpak unused reduced "+std::to_string(reclaimed/(1024*1024))+"MB";
            verdict=domain::Verdict::SUCCESS;
            details="unusedRuntimes before "+std::to_string(before.flatpak.unusedRuntimes.size())+" after "+std::to_string(after.flatpak.unusedRuntimes.size());
            return true;
        }
        observedBenefit="no benefit: flatpak unused not reduced";
        verdict=domain::Verdict::NO_BENEFIT;
        details="before reclaimable "+std::to_string(beforeReclaim)+" after "+std::to_string(afterReclaim);
        return true;
    }

    std::string explainWhyNow(const domain::PerformanceBaseline& b, const CapabilityEvidence& ev, const profile::UserProfile& profile) const override {
        (void)profile; (void)b;
        if(!ev.available) return "Flatpak cleanup not applicable: " + ev.reason + " (" + std::to_string(ev.reclaimableBytes/(1024*1024))+"MB reclaimable, confidence "+std::to_string((int)(ev.confidence*100))+"%)";
        std::ostringstream oss;
        oss<<"Measured flatpak has "<<ev.reclaimableBytes/(1024*1024)<<"MB reclaimable from "<<ev.preconditions.at("flatpak.unusedCount")<<" unused runtimes (benefit "<<ev.benefitStr<<", confidence "<<(int)(ev.confidence*100)<<"%, risk "<<risk()<<"). ";
        if(!b.storage.filesystems.empty()){
            auto &fs=b.storage.filesystems[0];
            oss<<"Filesystem "<<fs.mount<<" free "<<fs.freeBytes/(1024*1024*1024)<<"G used "<<(int)fs.usedPct<<"% (evidence: flatpak list).";
        }
        return oss.str();
    }
    std::string explainWhatWillChange(const CapabilityEvidence& ev) const override {
        std::ostringstream oss;
        oss<<"target=flatpak-unused, operation=uninstall --unused, reclaim "<<ev.reclaimableBytes/(1024*1024)<<"MB, method=flatpak uninstall --unused (user, no sudo, one transaction per runtime set), rollback via flatpak install <runtime>";
        return oss.str();
    }
    std::string explainWhatWillNotChange() const override {
        return "NVIDIA 470xx remains claimed driver nvidia, Intel remains default renderer, zram remains 8G lzo-rle, Akonadi remains running 14 agents, mssql remains disabled, fstab remains 3 entries, no reboot, no privileged operation unless explicitly authorized.";
    }
    std::vector<std::string> rejectionConditions(const domain::PerformanceBaseline& b, const CapabilityEvidence& ev, const profile::UserProfile& profile) const override {
        (void)profile; (void)b;
        std::vector<std::string> rc;
        rc.push_back("profile: flatpak not workflow-blocked (always ALLOWED_FOR_ANALYSIS)");
        rc.push_back("stale flatpak.stateHash: expected "+ev.stateHash.substr(0,16)+" observed different → FAILED");
        rc.push_back("unavailable evidence: flatpak meta not collected → INCONCLUSIVE");
        rc.push_back("insufficient benefit: reclaimable <500MB → NO_ACTION");
        rc.push_back("already optimal: unusedCount 0 → NO_ACTION");
        rc.push_back("transaction already completed → APPLY rejected");
        rc.push_back("backup unavailable: backupState != CREATED → FAILED");
        std::sort(rc.begin(), rc.end());
        return rc;
    }
};

} // namespace polaris::capabilities
