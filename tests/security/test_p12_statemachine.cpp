#include "../../core/safety/transaction/StateMachine.h"
#include <cassert>
#include <iostream>

using namespace polaris::safety;

void test_illegal_transitions_rejected(){
    // Must remain impossible examples from spec
    assert(!StateMachine::isValidTransition(TxState::COMPLETED, TxState::APPLYING));
    try { StateMachine::validateTransition(TxState::COMPLETED, TxState::APPLYING); assert(false); } catch(const std::logic_error& e){ std::string msg=e.what(); assert(msg.find("rejected, fail closed")!=std::string::npos); }
    std::cout << "COMPLETED -> APPLYING rejected PASS\n";

    assert(!StateMachine::isValidTransition(TxState::COMPLETED, TxState::APPROVED));
    std::cout << "COMPLETED -> APPROVED rejected PASS\n";

    assert(!StateMachine::isValidTransition(TxState::FAILED, TxState::APPLYING));
    std::cout << "FAILED -> APPLYING rejected PASS\n";

    assert(!StateMachine::isValidTransition(TxState::PREVIEWED, TxState::APPLYING));
    std::cout << "PREVIEWED -> APPLYING rejected PASS\n";

    assert(!StateMachine::isValidTransition(TxState::APPROVAL_REQUIRED, TxState::APPLYING));
    std::cout << "APPROVAL_REQUIRED -> APPLYING rejected PASS\n";

    assert(!StateMachine::isValidTransition(TxState::APPLYING, TxState::APPLYING));
    std::cout << "APPLYING -> APPLYING rejected PASS\n";

    // Additional illegal that must stay blocked
    assert(!StateMachine::isValidTransition(TxState::COMPLETED, TxState::CANCELLED));
    assert(!StateMachine::isValidTransition(TxState::ROLLED_BACK, TxState::APPLYING));
    assert(!StateMachine::isValidTransition(TxState::CANCELLED, TxState::APPLYING));
    assert(!StateMachine::isValidTransition(TxState::APPLIED, TxState::APPROVED));
    assert(!StateMachine::isValidTransition(TxState::VERIFYING, TxState::APPLYING));
    std::cout << "additional illegal transitions rejected PASS\n";

    // Valid transitions must remain possible
    assert(StateMachine::isValidTransition(TxState::PROPOSED, TxState::PREVIEWED));
    assert(StateMachine::isValidTransition(TxState::PREVIEWED, TxState::APPROVAL_REQUIRED));
    assert(StateMachine::isValidTransition(TxState::APPROVAL_REQUIRED, TxState::APPROVED));
    assert(StateMachine::isValidTransition(TxState::APPROVED, TxState::AUTHORIZATION_REQUIRED));
    assert(StateMachine::isValidTransition(TxState::AUTHORIZATION_REQUIRED, TxState::AUTHORIZED));
    assert(StateMachine::isValidTransition(TxState::AUTHORIZED, TxState::BACKUP_CREATED));
    assert(StateMachine::isValidTransition(TxState::BACKUP_CREATED, TxState::APPLYING));
    assert(StateMachine::isValidTransition(TxState::APPLYING, TxState::APPLIED));
    assert(StateMachine::isValidTransition(TxState::APPLIED, TxState::VERIFYING));
    assert(StateMachine::isValidTransition(TxState::VERIFYING, TxState::VERIFIED));
    assert(StateMachine::isValidTransition(TxState::VERIFIED, TxState::COMPLETED));
    std::cout << "valid happy-path transitions remain PASS\n";
}

void test_valid_recovery_remains(){
    // FAILED -> ROLLING_BACK -> ROLLED_BACK
    assert(StateMachine::isValidTransition(TxState::FAILED, TxState::ROLLING_BACK));
    assert(StateMachine::isValidTransition(TxState::ROLLING_BACK, TxState::ROLLED_BACK));
    std::cout << "FAILED -> ROLLING_BACK -> ROLLED_BACK PASS\n";

    assert(StateMachine::isValidTransition(TxState::FAILED, TxState::CANCELLED));
    std::cout << "FAILED -> CANCELLED PASS\n";

    // New P12 hardening: APPROVED -> FAILED should be valid for stale
    assert(StateMachine::isValidTransition(TxState::APPROVED, TxState::FAILED));
    std::cout << "APPROVED -> FAILED (stale) PASS (P12 hardening)\n";

    assert(StateMachine::isValidTransition(TxState::PREVIEWED, TxState::FAILED));
    assert(StateMachine::isValidTransition(TxState::APPROVAL_REQUIRED, TxState::FAILED));
    std::cout << "PREVIEWED/APPROVAL_REQUIRED -> FAILED PASS\n";

    // Terminal states remain terminal
    assert(StateMachine::isTerminal(TxState::COMPLETED));
    assert(StateMachine::isTerminal(TxState::ROLLED_BACK));
    assert(StateMachine::isTerminal(TxState::CANCELLED));
    assert(!StateMachine::isTerminal(TxState::FAILED));
    std::cout << "terminal states PASS\n";

    // Ensure illegal recovery not allowed: COMPLETED->ROLLING_BACK should be rejected
    assert(!StateMachine::isValidTransition(TxState::COMPLETED, TxState::ROLLING_BACK));
    // FAILED -> APPLYING remains blocked (must not auto-recover without explicit path)
    assert(!StateMachine::isValidTransition(TxState::FAILED, TxState::APPLYING));
    std::cout << "illegal recovery still blocked PASS\n";
}

void test_backup_created_needs_validation(){
    // Simulates that BACKUP_CREATED -> APPLYING without validation is blocked by validator, not SM
    // SM allows it, but store will check validation. We test SM allows, but validator would reject stale.
    // So this is not a SM transition, but a store guard. Ensure SM still allows valid
    assert(StateMachine::isValidTransition(TxState::BACKUP_CREATED, TxState::APPLYING));
    assert(StateMachine::isValidTransition(TxState::BACKUP_CREATED, TxState::FAILED));
    std::cout << "BACKUP_CREATED transitions PASS (APPLYING/FAILED)\n";
    // APPLYING -> APPLYING blocked
    assert(!StateMachine::isValidTransition(TxState::BACKUP_CREATED, TxState::BACKUP_CREATED));
}

int main(){
    test_illegal_transitions_rejected();
    test_valid_recovery_remains();
    test_backup_created_needs_validation();
    std::cout << "All P12 StateMachine hardening tests PASS (3 categories, 20+ transitions)\n";
    return 0;
}
