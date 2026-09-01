#include "../../core/capabilities/FlatpakUnusedCapability.h"
#include "../../core/providers/real/RealFlatpakProvider.h"
#include "../../core/domain/PerfModels.h"
#include "../../core/profile/UserProfile.h"
#include "../../core/safety/transaction/TransactionValidator.h"
#include <cassert>
#include <iostream>
#include <filesystem>

using namespace polaris::capabilities;
using namespace polaris::domain;
using namespace polaris::providers::real;
using namespace polaris::profile;

PerformanceBaseline makeFlatpakBaseline(const std::string& list, const std::string& unused, uint64_t freeBytes=50ULL*1024*1024*1024){
    PerformanceBaseline b;
    b.timestamp="2026-09-01T00:00:00+0330";
    b.flatpak = RealFlatpakProvider::fromFixture(list, unused);
    StorageBaseline::Fs fs;
    fs.mount="/"; fs.device="/dev/nvme0n1p3"; fs.fstype="ext4"; fs.sizeBytes=100ULL*1024*1024*1024; fs.freeBytes=freeBytes; fs.usedPct=50;
    b.storage.filesystems.push_back(fs);
    return b;
}

void test_unavailable_evidence(){
    FlatpakUnusedCapability cap;
    PerformanceBaseline b;
    b.timestamp="2026-09-01T00:00:00+0330";
    b.flatpak.meta.available=false;
    b.flatpak.meta.note="unavailable: flatpak not installed";
    auto ev = cap.collect(b);
    assert(!ev.available);
    assert(ev.reason.find("unavailable")!=std::string::npos);
    assert(ev.confidence==0.0);
    assert(ev.reclaimableBytes==0);
    UserProfile p;
    assert(!cap.isApplicable(b,p));
    std::cout << "unavailable evidence PASS\n";
}

void test_insufficient_evidence_no_unused(){
    FlatpakUnusedCapability cap;
    // list with only 1 runtime, no unused
    std::string list="Application Branch Origin InstalledSize\norg.freedesktop.Platform 23.08 flathub 900 MB\n";
    std::string unused="";
    auto b = makeFlatpakBaseline(list, unused);
    auto ev = cap.collect(b);
    assert(!ev.available);
    assert(ev.reason.find("no unused")!=std::string::npos);
    UserProfile p;
    assert(!cap.isApplicable(b,p));
    std::cout << "insufficient no unused PASS\n";
}

void test_insufficient_small_reclaimable(){
    FlatpakUnusedCapability cap;
    // 1 unused but only 100MB (<500MB threshold) - use distinct id to avoid branch confusion
    std::string list="Application Branch Origin InstalledSize\norg.freedesktop.Platform 23.08 flathub 900 MB\norg.example.SmallApp 1.0 flathub 100 MB\n";
    std::string unused="org.example.SmallApp 1.0 flathub\n";
    auto b = makeFlatpakBaseline(list, unused);
    auto ev = cap.collect(b);
    UserProfile p;
    assert(!cap.isApplicable(b,p));
    // But if we force, ev should be available with small benefit
    assert(ev.available);
    assert(ev.reclaimableBytes==100ULL*1024*1024);
    assert(ev.confidence==0.60);
    std::cout << "insufficient small reclaimable PASS\n";
}

void test_measured_benefit(){
    FlatpakUnusedCapability cap;
    std::string list=
        "Application Branch Origin InstalledSize\n"
        "org.freedesktop.Platform 23.08 flathub 900 MB\n"
        "org.gnome.Platform 50 flathub 600 MB\n"
        "org.freedesktop.Platform 24.08 flathub 850 MB\n"
        "org.gnome.Platform 51 flathub 650 MB\n";
    std::string unused="org.freedesktop.Platform 23.08 flathub\norg.gnome.Platform 50 flathub\n";
    auto b = makeFlatpakBaseline(list, unused);
    auto ev = cap.collect(b);
    assert(ev.available);
    // reclaimable = 900+600=1500MB =1.464GB
    assert(ev.reclaimableBytes== 1500ULL*1024*1024);
    assert(ev.benefitGB>1.4 && ev.benefitGB<1.5);
    // confidence for >=1.5 would be 0.90, for 1.46 also 0.85? Let's check: 1.464 >=1.0 =>0.85, but <1.5 =>0.85 not 0.90
    assert(ev.confidence==0.85);
    assert(ev.benefitStr.find("GB")!=std::string::npos);
    assert(ev.stateHash.size()==64);
    UserProfile p;
    assert(cap.isApplicable(b,p));
    auto rec = cap.toRecommendation(ev,b);
    assert(rec.id=="REC-flatpak-unused");
    assert(rec.confidence==0.85f);
    assert(rec.riskLevel=="R1");
    assert(rec.category=="Storage");
    std::cout << "measured benefit PASS\n";
}

void test_large_benefit_confidence(){
    FlatpakUnusedCapability cap;
    std::string list=
        "Application Branch Origin InstalledSize\n"
        "org.freedesktop.Platform 23.08 flathub 900 MB\n"
        "org.freedesktop.Platform 24.08 flathub 850 MB\n"
        "org.gnome.Platform 50 flathub 600 MB\n"
        "org.gnome.Platform 51 flathub 650 MB\n"
        "org.kde.Platform 6.7 flathub 700 MB\n"
        "org.kde.Platform 6.8 flathub 750 MB\n";
    std::string unused="org.freedesktop.Platform 23.08\norg.gnome.Platform 50\norg.kde.Platform 6.7\n";
    auto b = makeFlatpakBaseline(list, unused);
    auto ev = cap.collect(b);
    assert(ev.reclaimableBytes==2200ULL*1024*1024);
    assert(ev.benefitGB>2.1);
    assert(ev.confidence==0.90);
    std::cout << "large benefit confidence PASS\n";
}

void test_transaction_generation(){
    FlatpakUnusedCapability cap;
    std::string list="Application Branch Origin InstalledSize\norg.freedesktop.Platform 23.08 flathub 900 MB\norg.gnome.Platform 50 flathub 600 MB\n";
    std::string unused="org.freedesktop.Platform 23.08\n";
    auto b = makeFlatpakBaseline(list, unused);
    auto ev = cap.collect(b);
    // Ensure reclaimable 900MB >500MB so applicable
    UserProfile p;
    assert(cap.isApplicable(b,p));
    auto rec = cap.toRecommendation(ev,b);
    auto cur = cap.snapshot(b,ev);
    auto tx = cap.toTransaction("TX-TEST-FLATPAK-001", rec, ev, cur);
    assert(tx.id=="TX-TEST-FLATPAK-001");
    assert(tx.operationId=="flatpak-unused");
    assert(tx.target=="/tmp/polaris-test-root/p19/flatpak-unused.state");
    assert(tx.riskLevel=="R1");
    assert(tx.state==polaris::safety::TxState::PREVIEWED);
    assert(!tx.previews.empty());
    assert(tx.previews[0].target=="/tmp/polaris-test-root/p19/flatpak-unused.state");
    assert(tx.beforeHash==ev.stateHash);
    assert(tx.preconditions.at("flatpak.reclaimableBytes")==std::to_string(900ULL*1024*1024));
    std::cout << "transaction generation PASS\n";
}

void test_stale_evidence(){
    FlatpakUnusedCapability cap;
    std::string list1="Application Branch Origin InstalledSize\norg.freedesktop.Platform 23.08 flathub 900 MB\n";
    std::string unused1="org.freedesktop.Platform 23.08\n";
    auto b1 = makeFlatpakBaseline(list1, unused1);
    auto ev1 = cap.collect(b1);
    auto cur1 = cap.snapshot(b1, ev1);
    auto tx = cap.toTransaction("TX-TEST-FLATPAK-STALE", cap.toRecommendation(ev1,b1), ev1, cur1);
    // Simulate approval binding + state advance
    polaris::safety::CurrentState curAtApproval = cur1;
    polaris::safety::TransactionValidator::bindApproval(tx, curAtApproval);
    tx.state = polaris::safety::TxState::APPROVED;
    assert(tx.approvedBeforeHash==ev1.stateHash);
    // Now create new baseline with different reclaimable (stale)
    std::string list2="Application Branch Origin InstalledSize\norg.freedesktop.Platform 23.08 flathub 900 MB\norg.gnome.Platform 50 flathub 600 MB\n";
    std::string unused2="org.freedesktop.Platform 23.08\norg.gnome.Platform 50\n";
    auto b2 = makeFlatpakBaseline(list2, unused2);
    auto ev2 = cap.collect(b2);
    auto cur2 = cap.snapshot(b2, ev2);
    // Validate stale
    auto res = polaris::safety::TransactionValidator::validateForApply(tx, cur2);
    assert(!res.valid);
    assert(res.failingField.find("precondition")!=std::string::npos || res.failingField=="beforeHash" || res.failingField.find("flatpak")!=std::string::npos || res.failingField.find("target")!=std::string::npos);
    std::cout << "stale evidence PASS\n";
}

void test_verify_success(){
    FlatpakUnusedCapability cap;
    std::string list="Application Branch Origin InstalledSize\norg.freedesktop.Platform 23.08 flathub 900 MB\norg.gnome.Platform 50 flathub 600 MB\n";
    std::string unused="org.freedesktop.Platform 23.08\n";
    auto before = makeFlatpakBaseline(list, unused, 50ULL*1024*1024*1024);
    auto after = makeFlatpakBaseline("Application Branch Origin InstalledSize\norg.gnome.Platform 50 flathub 600 MB\n", "", 51ULL*1024*1024*1024); // after free increased 1GB
    // Set after reclaimable 0
    before.flatpak.reclaimableBytes=900ULL*1024*1024;
    after.flatpak.reclaimableBytes=0;
    after.flatpak.meta.available=true;
    before.flatpak.meta.available=true;
    std::string observed;
    polaris::domain::Verdict verdict;
    std::string details;
    bool ok = cap.verify(before, after, observed, verdict, details);
    assert(ok);
    // Storage free increased -> SUCCESS or IMPROVED
    assert(verdict==polaris::domain::Verdict::SUCCESS || verdict==polaris::domain::Verdict::IMPROVED);
    assert(observed.find("reclaimed")!=std::string::npos || observed.find("free")!=std::string::npos);
    std::cout << "verify success PASS\n";
}

void test_explainability(){
    FlatpakUnusedCapability cap;
    std::string list="Application Branch Origin InstalledSize\norg.freedesktop.Platform 23.08 flathub 900 MB\n";
    std::string unused="org.freedesktop.Platform 23.08\n";
    auto b = makeFlatpakBaseline(list, unused);
    auto ev = cap.collect(b);
    UserProfile p;
    std::string why = cap.explainWhyNow(b, ev, p);
    assert(why.find("flatpak")!=std::string::npos || why.find("reclaimable")!=std::string::npos);
    std::string what = cap.explainWhatWillChange(ev);
    assert(what.find("flatpak")!=std::string::npos);
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
    test_unavailable_evidence();
    test_insufficient_evidence_no_unused();
    test_insufficient_small_reclaimable();
    test_measured_benefit();
    test_large_benefit_confidence();
    test_transaction_generation();
    test_stale_evidence();
    test_verify_success();
    test_explainability();
    std::cout << "All flatpak capability tests PASS (9 categories)\n";
    return 0;
}
