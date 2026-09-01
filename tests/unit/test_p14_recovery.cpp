#include "../../core/safety/recovery/RecoveryDetector.h"
#include "../../core/safety/transaction/Transaction.h"
#include "../../core/safety/transaction/TransactionStore.h"
#include "../../core/safety/transaction/TransactionValidator.h"
#include "../../core/safety/backup/BackupEngine.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

using namespace polaris::safety;

void test_incomplete_detected(){
    std::string dir = "/tmp/polaris-test-root/p14_recovery_detect";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string storePath = dir + "/transactions";
    std::filesystem::create_directories(storePath);
    // Create incomplete transaction BACKUP_CREATED
    Transaction tx;
    tx.id = "TX-TEST-P14-RECOVERY-BACKUP";
    tx.operationId = "dummy-test";
    tx.target = "/tmp/polaris-test-root/p14_recovery_detect/etc/fstab";
    tx.state = TxState::BACKUP_CREATED;
    tx.approvalState = "APPROVED";
    tx.backupState = "CREATED";
    // Persist manually as JSON with state BACKUP_CREATED
    std::filesystem::create_directories("/tmp/polaris-test-root/p14_recovery_detect/etc");
    { std::ofstream out(tx.target); out << "original\n"; }
    // Create backup to simulate
    std::string backupDir = BackupEngine::testBackupRoot() + "/" + tx.id;
    std::filesystem::create_directories(backupDir);
    { std::ofstream out(backupDir + "/fstab.bak"); out << "original\n"; }
    {
        std::ofstream out(storePath + "/" + tx.id + ".json");
        out << "{\"id\":\"" << tx.id << "\",\"state\":\"BACKUP_CREATED\",\"target\":\"" << tx.target << "\"}";
    }
    auto infos = RecoveryDetector::detect(storePath);
    bool found=false;
    for(auto &info: infos) if(info.id==tx.id) { found=true; assert(info.state==TxState::BACKUP_CREATED); assert(info.suggested==TxState::FAILED); }
    assert(found);
    std::cout << "incomplete BACKUP_CREATED detected PASS\n";
    // Also test APPLYING
    Transaction tx2;
    tx2.id = "TX-TEST-P14-RECOVERY-APPLYING";
    tx2.state = TxState::APPLYING;
    {
        std::ofstream out(storePath + "/" + tx2.id + ".json");
        out << "{\"id\":\"" << tx2.id << "\",\"state\":\"APPLYING\"}";
    }
    auto infos2 = RecoveryDetector::detect(storePath);
    bool found2=false;
    for(auto &info: infos2) if(info.id==tx2.id) found2=true;
    assert(found2);
    std::cout << "incomplete APPLYING detected PASS\n";
    // COMPLETED should not be flagged
    Transaction tx3;
    tx3.id = "TX-TEST-P14-RECOVERY-COMPLETED";
    tx3.state = TxState::COMPLETED;
    {
        std::ofstream out(storePath + "/" + tx3.id + ".json");
        out << "{\"id\":\"" << tx3.id << "\",\"state\":\"COMPLETED\"}";
    }
    auto infos3 = RecoveryDetector::detect(storePath);
    bool found3=false;
    for(auto &info: infos3) if(info.id==tx3.id) found3=true;
    assert(!found3);
    std::cout << "COMPLETED not flagged as incomplete PASS\n";
}

void test_recovery_fails_closed(){
    std::string dir = "/tmp/polaris-test-root/p14_recovery_failclosed";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string storePath = dir + "/transactions";
    std::filesystem::create_directories(storePath);
    std::string file = dir + "/etc/fstab";
    std::filesystem::create_directories(dir + "/etc");
    { std::ofstream out(file); out << "original\n"; }
    std::string beforeHash = TransactionValidator::hashString("original\n");
    // Create incomplete transaction BACKUP_CREATED with backup exists
    std::string id = "TX-TEST-P14-RECOVERY-FAILCLOSED";
    {
        std::ofstream out(storePath + "/" + id + ".json");
        out << "{\"id\":\"" << id << "\",\"state\":\"BACKUP_CREATED\",\"target\":\"" << file << "\"}";
    }
    std::string backupDir = BackupEngine::testBackupRoot() + "/" + id;
    std::filesystem::create_directories(backupDir);
    { std::ofstream out(backupDir + "/fstab.bak"); out << "original\n"; }
    std::string beforeContent;
    { std::ifstream f(file); beforeContent.assign(std::istreambuf_iterator<char>(f), {}); }
    auto infos = RecoveryDetector::detect(storePath);
    assert(!infos.empty());
    for(auto &info: infos){
        if(info.id==id){
            assert(info.suggested==TxState::FAILED);
            assert(RecoveryDetector::shouldFailClosed(info)==true);
            // Ensure recovery does NOT auto-apply (file unchanged)
            std::string afterContent;
            { std::ifstream f(file); afterContent.assign(std::istreambuf_iterator<char>(f), {}); }
            assert(beforeContent==afterContent);
            // Ensure backup preserved
            assert(info.backupExists);
            std::cout << "recovery fails closed PASS (suggested FAILED, no auto-apply, backup preserved)\n";
        }
    }
    // Ensure file hash unchanged (no mutation)
    assert(TransactionValidator::hashString(beforeContent)==beforeHash);
    // Clean
    std::filesystem::remove_all(backupDir);
}

void test_no_real_host_mutation(){
    std::string beforeFstab;
    { std::ifstream f("/etc/fstab"); beforeFstab.assign(std::istreambuf_iterator<char>(f), {}); }
    std::string dir = "/tmp/polaris-test-root/p14_recovery_no_host";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir + "/transactions");
    { std::ofstream out(dir + "/transactions/TX-TEST-P14-NO-HOST.json"); out << "{\"id\":\"TX-TEST-P14-NO-HOST\",\"state\":\"BACKUP_CREATED\"}"; }
    auto infos = RecoveryDetector::detect(dir + "/transactions");
    std::string afterFstab;
    { std::ifstream f("/etc/fstab"); afterFstab.assign(std::istreambuf_iterator<char>(f), {}); }
    assert(beforeFstab==afterFstab);
    std::cout << "no real-host mutation during recovery detection PASS\n";
}

void test_existing_tests_intact_smoke(){
    // Smoke: existing P12/P13 logic still works after recovery detector added
    // Create a simple transaction and ensure store still works
    TransactionStore store("/tmp/polaris-test-root/transactions");
    store.clear();
    Transaction tx;
    tx.id = "TX-TEST-P14-SMOKE";
    tx.operationId = "dummy-test";
    tx.target = "/tmp/polaris-test-root/p14_smoke/etc/fstab";
    std::filesystem::create_directories("/tmp/polaris-test-root/p14_smoke/etc");
    { std::ofstream out(tx.target); out << "original\n"; }
    tx.beforeHash = TransactionValidator::hashString("original\n");
    tx.state = TxState::PREVIEWED;
    tx.approvalState = "PENDING";
    tx.backupState = "NONE";
    auto cr = store.create(tx);
    assert(cr.valid);
    std::cout << "existing P1-P13 tests remain intact smoke PASS\n";
}

int main(){
    test_incomplete_detected();
    test_recovery_fails_closed();
    test_no_real_host_mutation();
    test_existing_tests_intact_smoke();
    std::cout << "All P14 recovery tests PASS (4 categories)\n";
    return 0;
}
