#include "../../core/profile/UserProfile.h"
#include "../../core/profile/ProfileStore.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>

using namespace polaris::profile;

void test_default_unknown(){
    UserProfile p;
    assert(p.isDefaultUnknown());
    assert(p.usesKMail==TriState::UNKNOWN);
    assert(p.usesKontact==TriState::UNKNOWN);
    assert(p.usesBluetooth==TriState::UNKNOWN);
    assert(p.usesAkonadi==TriState::UNKNOWN);
    std::cout << "default unknown state PASS\n";
}

void test_explicit_yes_no_unknown(){
    UserProfile p;
    p.setField("usesKMail", TriState::YES);
    assert(p.getField("usesKMail")==TriState::YES);
    p.setField("usesKMail", TriState::NO);
    assert(p.getField("usesKMail")==TriState::NO);
    p.setField("usesKMail", TriState::UNKNOWN);
    assert(p.getField("usesKMail")==TriState::UNKNOWN);
    p.setField("usesBluetooth", TriState::YES);
    assert(p.getField("usesBluetooth")==TriState::YES);
    p.setField("usesPrinting", TriState::NO);
    assert(p.getField("usesPrinting")==TriState::NO);
    std::cout << "explicit yes/no/unknown PASS\n";
}

void test_deterministic_serialization(){
    UserProfile p1;
    p1.setField("usesKMail", TriState::YES);
    p1.setField("usesBluetooth", TriState::NO);
    std::string j1 = p1.toJson();
    std::string j2 = p1.toJson();
    assert(j1==j2);
    // Keys sorted alphabetical
    assert(j1.find("\"usesAkonadi\":\"unknown\"")!=std::string::npos);
    assert(j1.find("\"usesKMail\":\"yes\"")!=std::string::npos);
    assert(j1.find("\"usesBluetooth\":\"no\"")!=std::string::npos);
    // Two profiles with same values produce same JSON
    UserProfile p2;
    p2.setField("usesBluetooth", TriState::NO);
    p2.setField("usesKMail", TriState::YES);
    assert(p1.toJson()==p2.toJson());
    std::cout << "deterministic serialization PASS\n";
    std::cout << "  json: " << j1 << "\n";
}

void test_load_save_round_trip(){
    std::string dir = "/tmp/polaris-test-root/p13_roundtrip";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/profile.json";
    UserProfile p;
    p.setField("usesKMail", TriState::YES);
    p.setField("usesKontact", TriState::NO);
    p.setField("usesBluetooth", TriState::UNKNOWN);
    ProfileStore::save(p, path);
    assert(std::filesystem::exists(path));
    UserProfile loaded = ProfileStore::load(path);
    assert(loaded==p);
    std::cout << "load/save round-trip PASS\n";
}

void test_missing_profile(){
    std::string path = "/tmp/polaris-test-root/p13_missing/profile.json";
    std::filesystem::remove_all("/tmp/polaris-test-root/p13_missing");
    // Ensure not exists
    assert(!ProfileStore::exists(path));
    UserProfile p = ProfileStore::load(path);
    assert(p.isDefaultUnknown());
    // Ensure file not created merely by reading
    assert(!ProfileStore::exists(path));
    std::cout << "missing profile treated as unknown and not auto-created PASS\n";
}

void test_malformed_profile(){
    std::string dir = "/tmp/polaris-test-root/p13_malformed";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/profile.json";
    // Write invalid JSON
    { std::ofstream out(path); out << "{not_json}"; }
    bool threw=false;
    try { UserProfile::fromJson("{not_json}"); } catch(...){ threw=true; }
    assert(threw);
    // Store load should also throw
    bool threwLoad=false;
    try { ProfileStore::load(path); } catch(...){ threwLoad=true; }
    assert(threwLoad);
    std::cout << "malformed profile throws PASS\n";
    // Clean up
    std::filesystem::remove(path);
    // Also test invalid TriState value
    bool threwValue=false;
    try { UserProfile::fromJson("{\"usesKMail\":\"maybe\"}"); } catch(...){ threwValue=true; }
    assert(threwValue);
    std::cout << "invalid TriState value throws PASS\n";
}

int main(){
    test_default_unknown();
    test_explicit_yes_no_unknown();
    test_deterministic_serialization();
    test_load_save_round_trip();
    test_missing_profile();
    test_malformed_profile();
    std::cout << "All P13 profile model tests PASS (6 categories)\n";
    return 0;
}
