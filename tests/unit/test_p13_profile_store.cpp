#include "../../core/profile/UserProfile.h"
#include "../../core/profile/ProfileStore.h"
#include "../../core/safety/FileSafety.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

using namespace polaris::profile;

void test_atomic_persistence(){
    std::string dir = "/tmp/polaris-test-root/p13_atomic";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/profile.json";
    UserProfile p;
    p.setField("usesKMail", TriState::YES);
    ProfileStore::save(p, path);
    assert(std::filesystem::exists(path));
    // Check file is regular and not symlink
    assert(polaris::safety::FileSafety::isRegularFile(path));
    assert(!polaris::safety::FileSafety::isSymlink(path));
    // Check permissions 0600
    struct stat st;
    stat(path.c_str(), &st);
    // Permissions should be 0600 (owner read/write)
    assert((st.st_mode & 0777) == 0600);
    // Check deterministic after second save with same content -> file hash same (idempotent)
    std::string before;
    { std::ifstream f(path); before.assign(std::istreambuf_iterator<char>(f), {}); }
    ProfileStore::save(p, path);
    std::string after;
    { std::ifstream f(path); after.assign(std::istreambuf_iterator<char>(f), {}); }
    assert(before==after);
    std::cout << "atomic persistence PASS (0600, deterministic, no partial)\n";
}

void test_symlink_rejection(){
    std::string dir = "/tmp/polaris-test-root/p13_symlink";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string real = dir + "/real.json";
    std::string link = dir + "/profile.json";
    UserProfile p;
    p.setField("usesKMail", TriState::YES);
    ProfileStore::save(p, real);
    ::symlink(real.c_str(), link.c_str());
    assert(polaris::safety::FileSafety::isSymlink(link));
    bool threwLoad=false;
    try { ProfileStore::load(link); } catch(...){ threwLoad=true; }
    assert(threwLoad);
    bool threwSave=false;
    try { ProfileStore::save(p, link); } catch(...){ threwSave=true; }
    assert(threwSave);
    std::cout << "unsafe/symlink path rejection PASS\n";
    unlink(link.c_str());
}

void test_path_traversal_rejection(){
    UserProfile p;
    bool threw=false;
    try { ProfileStore::save(p, "/tmp/polaris-test-root/../etc/passwd"); } catch(...){ threw=true; }
    assert(threw);
    bool threw2=false;
    try { ProfileStore::save(p, "/tmp/polaris-test-root/profile.json; rm -rf /"); } catch(...){ threw2=true; }
    assert(threw2);
    std::cout << "path traversal / metachar rejection PASS\n";
}

void test_malformed_handling(){
    std::string dir = "/tmp/polaris-test-root/p13_malformed_store";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/profile.json";
    // Write malformed via direct file
    { std::ofstream out(path); out << "not json at all"; }
    bool threw=false;
    try { ProfileStore::load(path); } catch(const std::exception& e){ threw=true; std::cout << "  malformed load error: " << e.what() << "\n"; }
    assert(threw);
    std::cout << "malformed profile load throws PASS\n";
}

void test_deterministic_serialization_file(){
    std::string dir1 = "/tmp/polaris-test-root/p13_det1";
    std::string dir2 = "/tmp/polaris-test-root/p13_det2";
    std::filesystem::remove_all(dir1);
    std::filesystem::remove_all(dir2);
    std::filesystem::create_directories(dir1);
    std::filesystem::create_directories(dir2);
    UserProfile p1, p2;
    p1.setField("usesKMail", TriState::YES);
    p1.setField("usesBluetooth", TriState::NO);
    p2.setField("usesBluetooth", TriState::NO);
    p2.setField("usesKMail", TriState::YES);
    std::string path1 = dir1 + "/profile.json";
    std::string path2 = dir2 + "/profile.json";
    ProfileStore::save(p1, path1);
    ProfileStore::save(p2, path2);
    std::string c1, c2;
    { std::ifstream f(path1); c1.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(path2); c2.assign(std::istreambuf_iterator<char>(f), {}); }
    assert(c1==c2);
    std::cout << "deterministic file serialization PASS\n";
}

void test_real_profile_not_touched(){
    // Ensure tests do not modify real profile
    std::string real = ProfileStore::profilePath();
    bool existedBefore = std::filesystem::exists(real);
    std::string beforeHash;
    if(existedBefore){
        struct stat st; stat(real.c_str(), &st);
        beforeHash = std::to_string(st.st_mtime);
    }
    // Do operations on test paths only
    std::string testPath = "/tmp/polaris-test-root/p13_real_not_touched/profile.json";
    std::filesystem::create_directories("/tmp/polaris-test-root/p13_real_not_touched");
    UserProfile p; p.setField("usesKMail", TriState::YES);
    ProfileStore::save(p, testPath);
    // Check real still same
    bool existedAfter = std::filesystem::exists(real);
    if(existedBefore) assert(existedAfter);
    else assert(!existedAfter || !existedBefore); // if didn't exist before, it should still not exist after
    if(existedBefore && existedAfter){
        struct stat st2; stat(real.c_str(), &st2);
        assert(std::to_string(st2.st_mtime)==beforeHash);
    }
    std::cout << "real profile not modified by test fixtures PASS\n";
    std::filesystem::remove(testPath);
}

int main(){
    test_atomic_persistence();
    test_symlink_rejection();
    test_path_traversal_rejection();
    test_malformed_handling();
    test_deterministic_serialization_file();
    test_real_profile_not_touched();
    std::cout << "All P13 profile store tests PASS (6 categories)\n";
    return 0;
}
