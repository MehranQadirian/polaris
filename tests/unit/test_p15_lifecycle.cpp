#include "../../core/safety/transaction/StateMachine.h"
#include "../../core/safety/transaction/Transaction.h"
#include "../../core/safety/transaction/TransactionValidator.h"
#include "../../core/safety/transaction/TransactionStore.h"
#include <cassert>
#include <iostream>
#include <vector>

using namespace polaris::safety;

struct TransitionCase {
    TxState from;
    TxState to;
    bool expectValid;
    std::string description;
};

void test_valid_transitions_table(){
    std::vector<TransitionCase> valid = {
        {TxState::PROPOSED, TxState::PREVIEWED, true, "PROPOSED→PREVIEWED"},
        {TxState::PREVIEWED, TxState::APPROVAL_REQUIRED, true, "PREVIEWED→APPROVAL_REQUIRED"},
        {TxState::APPROVAL_REQUIRED, TxState::APPROVED, true, "APPROVAL_REQUIRED→APPROVED"},
        {TxState::APPROVED, TxState::AUTHORIZATION_REQUIRED, true, "APPROVED→AUTHORIZATION_REQUIRED"},
        {TxState::AUTHORIZATION_REQUIRED, TxState::AUTHORIZED, true, "AUTHORIZATION_REQUIRED→AUTHORIZED"},
        {TxState::AUTHORIZED, TxState::BACKUP_CREATED, true, "AUTHORIZED→BACKUP_CREATED"},
        {TxState::BACKUP_CREATED, TxState::APPLYING, true, "BACKUP_CREATED→APPLYING"},
        {TxState::APPLYING, TxState::APPLIED, true, "APPLYING→APPLIED"},
        {TxState::APPLIED, TxState::VERIFYING, true, "APPLIED→VERIFYING"},
        {TxState::VERIFYING, TxState::VERIFIED, true, "VERIFYING→VERIFIED"},
        {TxState::VERIFIED, TxState::COMPLETED, true, "VERIFIED→COMPLETED"},
        {TxState::FAILED, TxState::ROLLING_BACK, true, "FAILED→ROLLING_BACK"},
        {TxState::ROLLING_BACK, TxState::ROLLED_BACK, true, "ROLLING_BACK→ROLLED_BACK"},
        {TxState::PREVIEWED, TxState::FAILED, true, "PREVIEWED→FAILED (P12 stale)"},
        {TxState::APPROVAL_REQUIRED, TxState::FAILED, true, "APPROVAL_REQUIRED→FAILED"},
        {TxState::APPROVED, TxState::FAILED, true, "APPROVED→FAILED"},
    };
    for(auto &c: valid){
        assert(StateMachine::isValidTransition(c.from, c.to)==c.expectValid);
        try { StateMachine::validateTransition(c.from, c.to); } catch(...){ assert(false && "valid transition should not throw"); }
    }
    std::cout << "valid transitions table PASS (" << valid.size() << " cases)\n";
}

void test_rejected_transitions_table(){
    std::vector<TransitionCase> rejected = {
        {TxState::COMPLETED, TxState::APPLYING, false, "COMPLETED→APPLYING"},
        {TxState::COMPLETED, TxState::APPROVED, false, "COMPLETED→APPROVED"},
        {TxState::FAILED, TxState::APPLYING, false, "FAILED→APPLYING"},
        {TxState::PREVIEWED, TxState::APPLYING, false, "PREVIEWED→APPLYING"},
        {TxState::APPROVAL_REQUIRED, TxState::APPLYING, false, "APPROVAL_REQUIRED→APPLYING"},
        {TxState::APPLYING, TxState::APPLYING, false, "APPLYING→APPLYING"},
        {TxState::COMPLETED, TxState::CANCELLED, false, "COMPLETED→CANCELLED"},
        {TxState::ROLLED_BACK, TxState::APPLYING, false, "ROLLED_BACK→APPLYING"},
        {TxState::CANCELLED, TxState::APPLYING, false, "CANCELLED→APPLYING"},
        {TxState::APPLIED, TxState::APPROVED, false, "APPLIED→APPROVED"},
        {TxState::VERIFYING, TxState::APPLYING, false, "VERIFYING→APPLYING"},
        {TxState::PROPOSED, TxState::APPLYING, false, "PROPOSED→APPLYING"},
    };
    for(auto &c: rejected){
        assert(StateMachine::isValidTransition(c.from, c.to)==false);
        bool threw=false;
        try { StateMachine::validateTransition(c.from, c.to); } catch(const std::logic_error& e){ threw=true; assert(std::string(e.what()).find("rejected, fail closed")!=std::string::npos); }
        assert(threw);
    }
    std::cout << "rejected transitions table PASS (" << rejected.size() << " cases, fail-closed)\n";
}

void test_stale_leads_to_failed(){
    std::string dir = "/tmp/polaris-test-root/p15_lifecycle_stale";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir + "/etc");
    std::string file = dir + "/etc/fstab";
    { std::ofstream out(file); out << "original\n"; }
    Transaction tx;
    tx.id = "TX-TEST-P15-STALE-FAILED";
    tx.operationId = "dummy-test";
    tx.target = file;
    tx.beforeHash = TransactionValidator::hashString("original\n");
    tx.state = TxState::PREVIEWED;
    tx.approvalState = "PENDING";
    tx.backupState = "NONE";
    CurrentState cur;
    cur.currentBeforeHash = tx.beforeHash;
    cur.currentTarget = file;
    cur.currentOperation = "dummy-test";
    cur.filePath = file;
    cur.currentCanonical = FileSafety::canonical(file);
    TransactionStore store("/tmp/polaris-test-root/transactions");
    store.clear();
    store.create(tx);
    store.approve(tx.id, cur);
    CurrentState stale = cur;
    stale.currentBeforeHash = TransactionValidator::hashString("stale\n");
    auto vr = TransactionValidator::validateForApply(*store.get(tx.id), stale);
    assert(!vr.valid);
    assert(vr.failingField=="beforeHash");
    // Store apply should lead to FAILED
    auto ap = store.apply(tx.id, stale, "after\n");
    assert(!ap.valid);
    auto after = store.get(tx.id);
    assert(after->state==TxState::FAILED);
    std::cout << "stale leads to FAILED PASS\n";
}

void test_unverifiable_leads_to_failed(){
    Transaction tx;
    tx.id = "TX-TEST-P15-UNVERIFIABLE";
    tx.operationId = "dummy-test";
    tx.target = "/tmp/polaris-test-root/p15_unverif/etc/fstab";
    tx.beforeHash = TransactionValidator::hashString("original\n");
    tx.beforeUnitHash = TransactionValidator::hashString("enabled:active");
    tx.approvedBeforeHash = tx.beforeHash;
    tx.approvedUnitHash = tx.beforeUnitHash;
    tx.approvedTarget = tx.target;
    tx.approvedOperation = tx.operationId;
    tx.state = TxState::APPROVED;
    tx.approvalState = "APPROVED";
    tx.backupState = "NONE";
    CurrentState cur;
    cur.currentBeforeHash = tx.beforeHash;
    cur.currentTarget = tx.target;
    cur.currentOperation = tx.operationId;
    cur.currentUnitHash = ""; // unavailable -> should be unverifiable -> fail-closed
    cur.filePath = tx.target;
    auto vr = TransactionValidator::validateForApply(tx, cur);
    assert(!vr.valid);
    assert(vr.auditOperation.find("unverifiable")!=std::string::npos || vr.failingField=="unitHash");
    std::cout << "unverifiable leads to FAILED PASS\n";
}

int main(){
    test_valid_transitions_table();
    test_rejected_transitions_table();
    test_stale_leads_to_failed();
    test_unverifiable_leads_to_failed();
    std::cout << "All P15 lifecycle tests PASS\n";
    return 0;
}
