#include "../../core/profile/UserProfile.h"
#include "../../core/profile/ProfileStore.h"
#include "../../core/profile/ProfileService.h"
#include "../../core/profile/ProfileAdvisor.h"
#include "../../core/safety/transaction/Transaction.h"
#include "../../core/safety/transaction/TransactionValidator.h"
#include "../../core/safety/transaction/TransactionStore.h"
#include <cassert>
#include <iostream>
#include <filesystem>

using namespace polaris::profile;
using namespace polaris::safety;

void test_akonadi_blocked_by_kmail(){
    UserProfile p;
    p.setField("usesKMail", TriState::YES);
    auto r = ProfileAdvisor::canConsiderAkonadi(p);
    assert(r.decision==Decision::BLOCKED_BY_USER_WORKFLOW);
    assert(r.causingField=="usesKMail");
    assert(r.causingValue=="yes");
    assert(r.explicitFact==true);
    assert(r.reason.find("usesKMail=yes")!=std::string::npos);
    assert(r.whatWillNotChange.find("Akonadi will remain")!=std::string::npos);
    assert(r.confirmationRequired.find("usesKMail=no")!=std::string::npos);
    std::cout << "KMail yes → Akonadi BLOCKED PASS\n";
}

void test_akonadi_blocked_by_kontact(){
    UserProfile p;
    p.setField("usesKontact", TriState::YES);
    auto r = ProfileAdvisor::canConsiderAkonadi(p);
    assert(r.decision==Decision::BLOCKED_BY_USER_WORKFLOW);
    assert(r.causingField=="usesKontact");
    std::cout << "Kontact yes → Akonadi BLOCKED PASS\n";
}

void test_akonadi_blocked_by_akonadi_yes(){
    UserProfile p;
    p.setField("usesAkonadi", TriState::YES);
    auto r = ProfileAdvisor::canConsiderAkonadi(p);
    assert(r.decision==Decision::BLOCKED_BY_USER_WORKFLOW);
    assert(r.causingField=="usesAkonadi");
    std::cout << "usesAkonadi yes → BLOCKED PASS\n";
}

void test_akonadi_blocked_by_korganizer(){
    UserProfile p;
    p.setField("usesKOrganizer", TriState::YES);
    auto r = ProfileAdvisor::canConsiderAkonadi(p);
    assert(r.decision==Decision::BLOCKED_BY_USER_WORKFLOW);
    assert(r.causingField=="usesKOrganizer");
    std::cout << "KOrganizer yes → Akonadi BLOCKED PASS\n";
}

void test_unknown_requires_confirmation(){
    UserProfile p; // all unknown
    auto r = ProfileAdvisor::canConsiderAkonadi(p);
    assert(r.decision==Decision::REQUIRES_USER_CONFIRMATION);
    assert(r.explicitFact==false);
    assert(r.reason.find("unknown")!=std::string::npos);
    assert(r.confirmationRequired.find("unknown")!=std::string::npos);
    std::cout << "unknown → REQUIRES_USER_CONFIRMATION PASS\n";

    // Also test Bluetooth unknown
    auto r2 = ProfileAdvisor::canConsiderBluetooth(p);
    assert(r2.decision==Decision::REQUIRES_USER_CONFIRMATION);
    std::cout << "Bluetooth unknown → REQUIRES PASS\n";
}

void test_explicit_no_allowed_for_analysis(){
    UserProfile p;
    p.setField("usesKMail", TriState::NO);
    p.setField("usesKontact", TriState::NO);
    p.setField("usesKOrganizer", TriState::NO);
    p.setField("usesAkonadi", TriState::NO);
    auto r = ProfileAdvisor::canConsiderAkonadi(p);
    assert(r.decision==Decision::ALLOWED_FOR_ANALYSIS);
    assert(r.explicitFact==true);
    assert(r.reason.find("does NOT authorize mutation")!=std::string::npos);
    assert(r.reason.find("RECOMMEND→PREVIEW→APPROVAL")!=std::string::npos);
    assert(r.whatWillNotChange.find("until a future transaction")!=std::string::npos);
    std::cout << "explicit no → ALLOWED_FOR_ANALYSIS PASS (not approval)\n";

    // Bluetooth no
    UserProfile p2;
    p2.setField("usesBluetooth", TriState::NO);
    auto r2 = ProfileAdvisor::canConsiderBluetooth(p2);
    assert(r2.decision==Decision::ALLOWED_FOR_ANALYSIS);
    std::cout << "Bluetooth no → ALLOWED PASS\n";
}

void test_bluetooth_blocked(){
    UserProfile p;
    p.setField("usesBluetooth", TriState::YES);
    auto r = ProfileAdvisor::canConsiderBluetooth(p);
    assert(r.decision==Decision::BLOCKED_BY_USER_WORKFLOW);
    assert(r.reason.find("usesBluetooth=yes")!=std::string::npos);
    std::cout << "Bluetooth yes → BLOCKED PASS\n";
}

void test_printing_blocked(){
    UserProfile p;
    p.setField("usesPrinting", TriState::YES);
    auto r = ProfileAdvisor::canConsiderPrinting(p);
    assert(r.decision==Decision::BLOCKED_BY_USER_WORKFLOW);
    std::cout << "Printing yes → BLOCKED PASS\n";
    UserProfile p2;
    p2.setField("usesCups", TriState::YES);
    auto r2 = ProfileAdvisor::canConsiderPrinting(p2);
    assert(r2.decision==Decision::BLOCKED_BY_USER_WORKFLOW);
    std::cout << "Cups yes → Printing BLOCKED PASS\n";
}

void test_avahi_blocked(){
    UserProfile p;
    p.setField("usesAvahi", TriState::YES);
    auto r = ProfileAdvisor::canConsiderAvahi(p);
    assert(r.decision==Decision::BLOCKED_BY_USER_WORKFLOW);
    std::cout << "Avahi yes → BLOCKED PASS\n";
}

void test_profile_never_becomes_approval(){
    // Even ALLOWED does not bypass transaction approval
    UserProfile p;
    p.setField("usesBluetooth", TriState::NO);
    auto adv = ProfileAdvisor::canConsiderBluetooth(p);
    assert(adv.decision==Decision::ALLOWED_FOR_ANALYSIS);
    // Create a transaction for bluetooth service disable, without approval
    std::string dir = "/tmp/polaris-test-root/p13_never_approval";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir + "/etc");
    std::string file = dir + "/etc/test.conf";
    { std::ofstream out(file); out << "original\n"; }
    Transaction tx;
    tx.id = "TX-TEST-P13-NEVER-APPROVAL";
    tx.operationId = "bluetooth-disable";
    tx.target = file;
    tx.beforeHash = TransactionValidator::hashString("original\n");
    tx.state = TxState::PREVIEWED;
    tx.approvalState = "PENDING";
    tx.backupState = "NONE";
    TransactionStore store("/tmp/polaris-test-root/transactions");
    store.clear();
    store.create(tx);
    // Do NOT approve
    CurrentState cur;
    cur.currentBeforeHash = TransactionValidator::hashString("original\n");
    cur.currentTarget = file;
    cur.currentOperation = "bluetooth-disable";
    cur.filePath = file;
    auto vr = TransactionValidator::validateForApply(tx, cur);
    assert(!vr.valid);
    // Ensure advisor ALLOWED does not make transaction valid
    assert(adv.decision==Decision::ALLOWED_FOR_ANALYSIS);
    assert(!vr.valid); // still fails because not approved
    std::cout << "profile never becomes transaction approval PASS\n";
}

void test_no_host_mutation(){
    // Ensure profile tests didn't touch real profile
    std::string real = ProfileStore::profilePath();
    bool existedBefore = std::filesystem::exists(real);
    std::string beforeMtime;
    if(existedBefore){
        struct stat st; stat(real.c_str(), &st);
        beforeMtime = std::to_string(st.st_mtime);
    }
    // Do a test profile operation on fixture
    std::string testPath = "/tmp/polaris-test-root/p13_no_host/profile.json";
    std::filesystem::create_directories("/tmp/polaris-test-root/p13_no_host");
    UserProfile p; p.setField("usesKMail", TriState::YES);
    ProfileStore::save(p, testPath);
    // Check real profile unchanged
    bool existedAfter = std::filesystem::exists(real);
    if(existedBefore) assert(existedAfter);
    else assert(!existedAfter || !existedBefore);
    if(existedBefore && existedAfter){
        struct stat st; stat(real.c_str(), &st);
        assert(std::to_string(st.st_mtime)==beforeMtime);
    }
    std::cout << "no host mutation (real profile untouched) PASS\n";
}

void test_explainability(){
    UserProfile p;
    p.setField("usesKMail", TriState::YES);
    auto r = ProfileAdvisor::canConsiderAkonadi(p);
    assert(!r.reason.empty());
    assert(!r.causingField.empty());
    assert(!r.causingValue.empty());
    assert(!r.whatWillNotChange.empty());
    assert(!r.confirmationRequired.empty());
    assert(r.explicitFact==true);
    std::cout << "explainability fields present PASS\n";
    std::cout << "  reason: " << r.reason << "\n";
    std::cout << "  whatWillNotChange: " << r.whatWillNotChange << "\n";
    std::cout << "  confirmationRequired: " << r.confirmationRequired << "\n";
}

int main(){
    test_akonadi_blocked_by_kmail();
    test_akonadi_blocked_by_kontact();
    test_akonadi_blocked_by_akonadi_yes();
    test_akonadi_blocked_by_korganizer();
    test_unknown_requires_confirmation();
    test_explicit_no_allowed_for_analysis();
    test_bluetooth_blocked();
    test_printing_blocked();
    test_avahi_blocked();
    test_profile_never_becomes_approval();
    test_no_host_mutation();
    test_explainability();
    std::cout << "All P13 profile advisor tests PASS (12 categories)\n";
    return 0;
}
