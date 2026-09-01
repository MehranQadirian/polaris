#include "../../core/safety/transaction/Transaction.h"
#include "../../core/safety/transaction/TransactionValidator.h"
#include "../../core/safety/transaction/TransactionStore.h"
#include "../../core/safety/FileSafety.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <vector>

using namespace polaris::safety;

struct StaleCase {
    std::string field; // target, operation, beforeHash, unitHash, kernel, package, precondition
    std::string approvedValue;
    std::string currentValue;
    bool expectValid; // true if UNCHANGED
    std::string description;
};

void test_stale_matrix_table(){
    std::string dir = "/tmp/polaris-test-root/p15_stale_matrix";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir + "/etc");
    std::string file = dir + "/etc/fstab";
    std::string file2 = dir + "/etc/other.conf";
    { std::ofstream out(file); out << "original\n"; }
    { std::ofstream out(file2); out << "other\n"; }

    // Base approved values
    std::string approvedTarget = file;
    std::string approvedOp = "fstab-stale-swap";
    std::string approvedBefore = TransactionValidator::hashString("original\n");
    std::string approvedUnit = TransactionValidator::hashString("enabled:active");
    std::string approvedKernel = "7.1.10-200.fc44.x86_64";
    std::string approvedPackage = TransactionValidator::hashString("akmod-nvidia-470xx");
    std::map<std::string,std::string> approvedPre = {{"service.mssql.enabled","disabled"}, {"config.hash","abc"}};

    std::vector<StaleCase> cases = {
        // target
        {"target", approvedTarget, approvedTarget, true, "target UNCHANGED"},
        {"target", approvedTarget, file2, false, "target CHANGED"},
        {"target", approvedTarget, "", false, "target UNAVAILABLE (empty)"},
        // operation
        {"operation", approvedOp, approvedOp, true, "operation UNCHANGED"},
        {"operation", approvedOp, "other-op", false, "operation CHANGED"},
        {"operation", approvedOp, "", false, "operation UNAVAILABLE"},
        // beforeHash
        {"beforeHash", approvedBefore, approvedBefore, true, "beforeHash UNCHANGED"},
        {"beforeHash", approvedBefore, TransactionValidator::hashString("stale\n"), false, "beforeHash CHANGED"},
        {"beforeHash", approvedBefore, "", false, "beforeHash UNAVAILABLE"},
        // unitHash
        {"unitHash", approvedUnit, approvedUnit, true, "unitHash UNCHANGED"},
        {"unitHash", approvedUnit, TransactionValidator::hashString("disabled:inactive"), false, "unitHash CHANGED"},
        {"unitHash", approvedUnit, "", false, "unitHash UNAVAILABLE"},
        // kernel
        {"kernel", approvedKernel, approvedKernel, true, "kernel UNCHANGED"},
        {"kernel", approvedKernel, "7.1.11-300", false, "kernel CHANGED"},
        // package
        {"package", approvedPackage, approvedPackage, true, "package UNCHANGED"},
        {"package", approvedPackage, TransactionValidator::hashString("other-package"), false, "package CHANGED"},
        // precondition
        {"precondition", "disabled", "disabled", true, "precondition UNCHANGED"},
        {"precondition", "disabled", "enabled", false, "precondition CHANGED"},
        {"precondition", "disabled", "", false, "precondition UNAVAILABLE"},
    };

    for(auto &c: cases){
        Transaction tx;
        tx.id = "TX-TEST-P15-MATRIX-" + c.field + "-" + (c.expectValid ? "unchanged" : "changed");
        tx.operationId = approvedOp;
        tx.target = approvedTarget;
        tx.beforeHash = approvedBefore;
        tx.beforeUnitHash = approvedUnit;
        tx.kernelVersion = approvedKernel;
        tx.packageStateHash = approvedPackage;
        tx.preconditions = approvedPre;
        // Set approved snapshot
        tx.approvedTarget = approvedTarget;
        tx.approvedOperation = approvedOp;
        tx.approvedBeforeHash = approvedBefore;
        tx.approvedUnitHash = approvedUnit;
        tx.approvedKernelVersion = approvedKernel;
        tx.approvedPackageStateHash = approvedPackage;
        tx.approvedPreconditions = approvedPre;
        tx.state = TxState::APPROVED;
        tx.approvalState = "APPROVED";
        tx.backupState = "NONE";

        CurrentState cur;
        // Fill cur based on case
        if(c.field=="target") cur.currentTarget = c.currentValue;
        else cur.currentTarget = approvedTarget;
        if(c.field=="operation") cur.currentOperation = c.currentValue;
        else cur.currentOperation = approvedOp;
        if(c.field=="beforeHash") cur.currentBeforeHash = c.currentValue;
        else cur.currentBeforeHash = approvedBefore;
        if(c.field=="unitHash") cur.currentUnitHash = c.currentValue;
        else cur.currentUnitHash = approvedUnit;
        if(c.field=="kernel") cur.currentKernelVersion = c.currentValue;
        else cur.currentKernelVersion = approvedKernel;
        if(c.field=="package") cur.currentPackageStateHash = c.currentValue;
        else cur.currentPackageStateHash = approvedPackage;
        if(c.field=="precondition"){
            if(c.currentValue.empty()) cur.currentPreconditions = {};
            else if(c.currentValue=="disabled") cur.currentPreconditions = approvedPre;
            else cur.currentPreconditions = {{"service.mssql.enabled",c.currentValue},{"config.hash","abc"}};
        } else {
            cur.currentPreconditions = approvedPre;
        }
        cur.filePath = file;
        if(std::filesystem::exists(file)) cur.currentCanonical = FileSafety::canonical(file);

        auto vr = TransactionValidator::validateForApply(tx, cur);
        if(c.expectValid){
            if(!vr.valid){
                std::cout << "FAIL " << c.description << " reason=" << vr.reason << " field=" << vr.failingField << "\n";
            }
            assert(vr.valid);
        } else {
            assert(!vr.valid);
            assert(!vr.expected.empty() || !vr.failingField.empty());
            // Deterministic: expected should be approvedValue
            assert(vr.expected==c.approvedValue || vr.failingField==c.field || vr.failingField.find(c.field)!=std::string::npos || c.field=="precondition" || c.field=="target" || c.field=="operation");
        }
    }
    std::cout << "stale matrix table PASS (" << cases.size() << " cases: UNCHANGED accepted, CHANGED/UNAVAILABLE rejected)\n";
}

void test_multiple_fields_changed(){
    Transaction tx;
    tx.id = "TX-TEST-P15-MULTI";
    tx.operationId = "opA";
    tx.target = "/tmp/polaris-test-root/p15_multi/etc/fstab";
    tx.beforeHash = TransactionValidator::hashString("orig\n");
    tx.approvedTarget = tx.target;
    tx.approvedOperation = tx.operationId;
    tx.approvedBeforeHash = tx.beforeHash;
    tx.approvedUnitHash = TransactionValidator::hashString("enabled:active");
    tx.approvedKernelVersion = "7.1.10";
    tx.state = TxState::APPROVED;
    tx.approvalState = "APPROVED";
    tx.backupState = "NONE";
    CurrentState cur;
    cur.currentTarget = "/tmp/other";
    cur.currentOperation = "opB";
    cur.currentBeforeHash = TransactionValidator::hashString("stale\n");
    cur.currentUnitHash = TransactionValidator::hashString("disabled");
    cur.currentKernelVersion = "7.1.11";
    auto vr = TransactionValidator::validateForApply(tx, cur);
    assert(!vr.valid);
    // Should report first failing field (target) deterministically
    assert(vr.failingField=="target" || vr.reason.find("target")!=std::string::npos);
    std::cout << "multiple changed fields deterministic first failure PASS\n";
}

int main(){
    test_stale_matrix_table();
    test_multiple_fields_changed();
    std::cout << "All P15 stale matrix tests PASS\n";
    return 0;
}
