#include "../../core/capabilities/JournalVacuumCapability.h"
#include "../../core/providers/real/RealJournalDiskProvider.h"
#include "../../core/domain/PerfModels.h"
#include "../../core/profile/UserProfile.h"
#include "../../core/safety/transaction/TransactionValidator.h"
#include <cassert>
#include <iostream>

using namespace polaris::capabilities;
using namespace polaris::domain;
using namespace polaris::providers::real;
using namespace polaris::profile;

PerformanceBaseline makeJournalBaseline(const std::string& usageStr, uint64_t freeBytes=50ULL*1024*1024*1024){
    PerformanceBaseline b;
    b.timestamp="2026-09-01T00:00:00+0330";
    b.journalDisk = RealJournalDiskProvider::fromFixture(usageStr, "500M");
    StorageBaseline::Fs fs;
    fs.mount="/"; fs.device="/dev/nvme0n1p3"; fs.fstype="ext4"; fs.sizeBytes=100ULL*1024*1024*1024; fs.freeBytes=freeBytes; fs.usedPct=50;
    b.storage.filesystems.push_back(fs);
    b.journal.p3count=10;
    b.journal.families.push_back({"other",5,"example"});
    return b;
}

void test_unavailable(){
    JournalVacuumCapability cap;
    PerformanceBaseline b;
    b.timestamp="2026-09-01T00:00:00+0330";
    b.journalDisk.meta.available=false;
    b.journalDisk.meta.note="unavailable";
    auto ev = cap.collect(b);
    assert(!ev.available);
    assert(ev.reason.find("unavailable")!=std::string::npos);
    UserProfile p;
    assert(!cap.isApplicable(b,p));
    std::cout << "unavailable PASS\n";
}

void test_insufficient_small_usage(){
    JournalVacuumCapability cap;
    auto b = makeJournalBaseline("Archived and active journals take up 800M in the file system.");
    auto ev = cap.collect(b);
    assert(!ev.available);
    assert(ev.reason.find("<1GB")!=std::string::npos || ev.reason.find("800")!=std::string::npos);
    UserProfile p;
    assert(!cap.isApplicable(b,p));
    std::cout << "insufficient small usage PASS\n";
}

void test_insufficient_small_reclaimable(){
    JournalVacuumCapability cap;
    // 700M usage is <1GB threshold, so not applicable due to usage <1GB
    auto b = makeJournalBaseline("Archived and active journals take up 700M in the file system.");
    auto ev = cap.collect(b);
    assert(!ev.available);
    // Reason will be about usage <1GB
    assert(ev.reason.find("<1GB")!=std::string::npos || ev.reason.find("700")!=std::string::npos || ev.reason.find("reclaimable")!=std::string::npos);
    assert(!cap.isApplicable(b, UserProfile{}));
    std::cout << "insufficient small reclaimable PASS\n";
}

void test_measured_benefit(){
    JournalVacuumCapability cap;
    auto b = makeJournalBaseline("Archived and active journals take up 3.2G in the file system.");
    auto ev = cap.collect(b);
    assert(ev.available);
    assert(ev.reclaimableBytes > 2ULL*1024*1024*1024);
    // 3.2G = 3435973836, minus 500M 524288000 = 2911685836 ~2.71GB
    assert(ev.benefitGB>2.5 && ev.benefitGB<3.0);
    assert(ev.confidence==0.90);
    assert(ev.stateHash.size()==64);
    UserProfile p;
    assert(cap.isApplicable(b,p));
    auto rec = cap.toRecommendation(ev,b);
    assert(rec.id=="REC-journal-vacuum");
    assert(rec.riskLevel=="R1");
    assert(rec.requiresAuth==true);
    assert(rec.category=="Storage");
    std::cout << "measured benefit PASS\n";
}

void test_medium_benefit(){
    JournalVacuumCapability cap;
    auto b = makeJournalBaseline("Archived and active journals take up 1.5G in the file system.");
    auto ev = cap.collect(b);
    assert(ev.available);
    // 1.5G -500M =1.0G
    assert(ev.benefitGB>0.9 && ev.benefitGB<1.1);
    assert(ev.confidence==0.85);
    std::cout << "medium benefit PASS\n";
}

void test_transaction_generation(){
    JournalVacuumCapability cap;
    auto b = makeJournalBaseline("Archived and active journals take up 2.5G in the file system.");
    auto ev = cap.collect(b);
    UserProfile p;
    assert(cap.isApplicable(b,p));
    auto rec = cap.toRecommendation(ev,b);
    auto cur = cap.snapshot(b,ev);
    auto tx = cap.toTransaction("TX-TEST-JOURNAL-001", rec, ev, cur);
    assert(tx.operationId=="journal-vacuum");
    assert(tx.target=="/tmp/polaris-test-root/p19/journal-vacuum.state");
    assert(tx.riskLevel=="R1");
    assert(tx.requiredPrivileges=="org.polaris.journal.vacuum");
    assert(tx.state==polaris::safety::TxState::PREVIEWED);
    assert(!tx.previews.empty());
    assert(tx.beforeHash==ev.stateHash);
    assert(tx.preconditions.at("journal.diskUsageBytes")==std::to_string(2ULL*1024*1024*1024 + 512*1024*1024)); // approx 2.5G
    std::cout << "transaction generation PASS\n";
}

void test_stale(){
    JournalVacuumCapability cap;
    auto b1 = makeJournalBaseline("Archived and active journals take up 3.2G in the file system.");
    auto ev1 = cap.collect(b1);
    auto cur1 = cap.snapshot(b1, ev1);
    auto tx = cap.toTransaction("TX-TEST-JOURNAL-STALE", cap.toRecommendation(ev1,b1), ev1, cur1);
    polaris::safety::CurrentState curAtApproval = cur1;
    polaris::safety::TransactionValidator::bindApproval(tx, curAtApproval);
    tx.state = polaris::safety::TxState::APPROVED;
    assert(tx.approvedBeforeHash==ev1.stateHash);
    auto b2 = makeJournalBaseline("Archived and active journals take up 1.1G in the file system.");
    auto ev2 = cap.collect(b2);
    auto cur2 = cap.snapshot(b2, ev2);
    auto res = polaris::safety::TransactionValidator::validateForApply(tx, cur2);
    assert(!res.valid);
    // Should be stale precondition or beforeHash
    assert(res.failingField.find("precondition")!=std::string::npos || res.failingField=="beforeHash" || res.failingField.find("journal")!=std::string::npos || res.failingField.find("target")!=std::string::npos);
    std::cout << "stale PASS\n";
}

void test_verify_success(){
    JournalVacuumCapability cap;
    auto before = makeJournalBaseline("Archived and active journals take up 3.2G in the file system.");
    auto after = makeJournalBaseline("Archived and active journals take up 400M in the file system.");
    std::string observed; Verdict verdict; std::string details;
    bool ok = cap.verify(before, after, observed, verdict, details);
    assert(ok);
    assert(verdict==Verdict::SUCCESS || verdict==Verdict::IMPROVED);
    assert(observed.find("reclaimed")!=std::string::npos);
    std::cout << "verify success PASS\n";
}

void test_verify_no_change(){
    JournalVacuumCapability cap;
    auto before = makeJournalBaseline("Archived and active journals take up 3.2G in the file system.");
    auto after = makeJournalBaseline("Archived and active journals take up 3.2G in the file system.");
    std::string observed; Verdict verdict; std::string details;
    bool ok = cap.verify(before, after, observed, verdict, details);
    assert(ok);
    assert(verdict==Verdict::NO_CHANGE);
    std::cout << "verify no change PASS\n";
}

void test_explainability(){
    JournalVacuumCapability cap;
    auto b = makeJournalBaseline("Archived and active journals take up 3.2G in the file system.");
    auto ev = cap.collect(b);
    UserProfile p;
    std::string why = cap.explainWhyNow(b, ev, p);
    assert(why.find("journal")!=std::string::npos);
    std::string what = cap.explainWhatWillChange(ev);
    assert(what.find("journal")!=std::string::npos);
    std::string notChange = cap.explainWhatWillNotChange();
    assert(notChange.find("NVIDIA")!=std::string::npos);
    auto rc = cap.rejectionConditions(b, ev, p);
    assert(!rc.empty());
    bool hasStale=false;
    for(auto &c: rc) if(c.find("stale")!=std::string::npos) hasStale=true;
    assert(hasStale);
    std::cout << "explainability PASS\n";
}

int main(){
    test_unavailable();
    test_insufficient_small_usage();
    test_insufficient_small_reclaimable();
    test_measured_benefit();
    test_medium_benefit();
    test_transaction_generation();
    test_stale();
    test_verify_success();
    test_verify_no_change();
    test_explainability();
    std::cout << "All journal capability tests PASS (10 categories)\n";
    return 0;
}
