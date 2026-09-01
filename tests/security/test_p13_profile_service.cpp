#include "../../core/profile/UserProfile.h"
#include "../../core/profile/ProfileStore.h"
#include "../../core/profile/ProfileService.h"
#include "../../core/safety/audit/AuditLog.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>

using namespace polaris::profile;

void test_explicit_updates(){
    std::string dir = "/tmp/polaris-test-root/p13_service_explicit";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/profile.json";
    UserProfile p;
    auto r1 = ProfileService::updateField(p, "usesKMail", TriState::YES, path);
    assert(r1.success);
    assert(r1.field=="usesKMail");
    assert(r1.previousValue==TriState::UNKNOWN);
    assert(r1.newValue==TriState::YES);
    assert(r1.auditOperation=="profile.updated");
    assert(p.getField("usesKMail")==TriState::YES);
    // Verify persisted
    UserProfile loaded = ProfileStore::load(path);
    assert(loaded.getField("usesKMail")==TriState::YES);
    std::cout << "explicit yes PASS\n";

    auto r2 = ProfileService::updateField(p, "usesBluetooth", "no", path);
    assert(r2.newValue==TriState::NO);
    assert(p.getField("usesBluetooth")==TriState::NO);
    std::cout << "explicit no via string PASS\n";

    auto r3 = ProfileService::updateField(p, "usesKMail", "unknown", path);
    assert(r3.newValue==TriState::UNKNOWN);
    assert(p.getField("usesKMail")==TriState::UNKNOWN);
    std::cout << "explicit unknown PASS\n";
}

void test_no_inference(){
    std::string dir = "/tmp/polaris-test-root/p13_no_inference";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/profile.json";
    UserProfile p;
    ProfileService::updateField(p, "usesKMail", TriState::YES, path);
    // Check that usesAkonadi is still UNKNOWN, not inferred
    assert(p.getField("usesAkonadi")==TriState::UNKNOWN);
    assert(p.getField("usesKontact")==TriState::UNKNOWN);
    std::cout << "no inference KMail->Akonadi PASS\n";

    // Set Bluetooth, ensure Avahi not inferred
    ProfileService::updateField(p, "usesBluetooth", TriState::YES, path);
    assert(p.getField("usesAvahi")==TriState::UNKNOWN);
    std::cout << "no inference Bluetooth->Avahi PASS\n";
}

void test_audit_event_generation(){
    std::string dir = "/tmp/polaris-test-root/p13_audit";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/profile.json";
    std::filesystem::remove("/tmp/polaris-test-root/audit.log");
    UserProfile p;
    ProfileService::updateField(p, "usesKMail", TriState::YES, path);
    auto events = polaris::safety::AuditLog::list("PROFILE");
    bool found=false;
    for(auto &e: events){
        if(e.error.find("profile.updated")!=std::string::npos || e.error.find("usesKMail")!=std::string::npos) {
            if(e.error.find("previous=unknown")!=std::string::npos && e.error.find("new=yes")!=std::string::npos) found=true;
        }
        // Also check operation in raw
        if(e.error.find("field=usesKMail")!=std::string::npos) found=true;
    }
    // More direct: check via list raw
    bool foundOp=false;
    std::ifstream f("/tmp/polaris-test-root/audit.log");
    std::string line;
    while(std::getline(f,line)){
        if(line.find("profile.updated")!=std::string::npos && line.find("usesKMail")!=std::string::npos) foundOp=true;
    }
    assert(foundOp || found);
    std::cout << "audit event generation PASS\n";
}

void test_idempotency(){
    std::string dir = "/tmp/polaris-test-root/p13_idempotent";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/profile.json";
    UserProfile p;
    ProfileService::updateField(p, "usesKMail", TriState::YES, path);
    struct stat st1; stat(path.c_str(), &st1);
    // Update same value again
    auto r2 = ProfileService::updateField(p, "usesKMail", TriState::YES, path);
    assert(r2.auditOperation=="profile.update.idempotent");
    assert(r2.previousValue==TriState::YES && r2.newValue==TriState::YES);
    struct stat st2; stat(path.c_str(), &st2);
    // File should not be rewritten (mtime same) or at least content same
    // Our service skips write on idempotent, so mtime should be same
    assert(st1.st_mtime==st2.st_mtime);
    std::cout << "profile update idempotency PASS (no rewrite, audit idempotent)\n";
}

void test_unknown_field_rejected(){
    UserProfile p;
    bool threw=false;
    try { ProfileService::updateField(p, "usesUnknownField", TriState::YES, "/tmp/polaris-test-root/p13_unknown/profile.json"); } catch(...){ threw=true; }
    assert(threw);
    std::cout << "unknown field rejected PASS\n";
}

void test_explicit_vs_unknown(){
    UserProfile p;
    // Initially unknown
    assert(p.getField("usesKMail")==TriState::UNKNOWN);
    // After explicit set, it's explicit
    std::string dir = "/tmp/polaris-test-root/p13_explicit";
    std::filesystem::create_directories(dir);
    std::string path = dir + "/profile.json";
    ProfileService::updateField(p, "usesKMail", TriState::YES, path);
    assert(p.getField("usesKMail")==TriState::YES);
    // Check that profile distinguishes explicit vs unknown
    UserProfile p2;
    assert(p2.getField("usesKMail")==TriState::UNKNOWN);
    assert(p != p2);
    std::cout << "explicit vs unknown semantics PASS\n";
}

int main(){
    // Ensure clean audit log for this suite
    std::filesystem::remove("/tmp/polaris-test-root/audit.log");
    test_explicit_updates();
    test_no_inference();
    test_audit_event_generation();
    test_idempotency();
    test_unknown_field_rejected();
    test_explicit_vs_unknown();
    std::cout << "All P13 profile service tests PASS (6 categories)\n";
    return 0;
}
