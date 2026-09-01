#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <stdexcept>
#include <algorithm>

namespace polaris::safety {

// P4 Transaction State Machine - formally validated, no ambiguous bool
enum class TxState {
    PROPOSED,
    PREVIEWED,
    APPROVAL_REQUIRED,
    APPROVED,
    AUTHORIZATION_REQUIRED,
    AUTHORIZED,
    BACKUP_CREATED,
    APPLYING,
    APPLIED,
    VERIFYING,
    VERIFIED,
    FAILED,
    ROLLING_BACK,
    ROLLED_BACK,
    COMPLETED,
    CANCELLED
};

inline std::string toString(TxState s){
    switch(s){
        case TxState::PROPOSED: return "PROPOSED";
        case TxState::PREVIEWED: return "PREVIEWED";
        case TxState::APPROVAL_REQUIRED: return "APPROVAL_REQUIRED";
        case TxState::APPROVED: return "APPROVED";
        case TxState::AUTHORIZATION_REQUIRED: return "AUTHORIZATION_REQUIRED";
        case TxState::AUTHORIZED: return "AUTHORIZED";
        case TxState::BACKUP_CREATED: return "BACKUP_CREATED";
        case TxState::APPLYING: return "APPLYING";
        case TxState::APPLIED: return "APPLIED";
        case TxState::VERIFYING: return "VERIFYING";
        case TxState::VERIFIED: return "VERIFIED";
        case TxState::FAILED: return "FAILED";
        case TxState::ROLLING_BACK: return "ROLLING_BACK";
        case TxState::ROLLED_BACK: return "ROLLED_BACK";
        case TxState::COMPLETED: return "COMPLETED";
        case TxState::CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

class StateMachine {
public:
    // P12 hardening: PREVIEWED/APPROVAL_REQUIRED/APPROVED can now transition to FAILED on stale validation
    // This preserves fail-closed behavior (adds safe failure paths, does not weaken restrictions)
    static bool isValidTransition(TxState from, TxState to){
        static const std::map<TxState, std::vector<TxState>> allowed = {
            {TxState::PROPOSED, {TxState::PREVIEWED, TxState::CANCELLED}},
            {TxState::PREVIEWED, {TxState::APPROVAL_REQUIRED, TxState::FAILED, TxState::CANCELLED}},
            {TxState::APPROVAL_REQUIRED, {TxState::APPROVED, TxState::FAILED, TxState::CANCELLED}},
            {TxState::APPROVED, {TxState::AUTHORIZATION_REQUIRED, TxState::FAILED, TxState::CANCELLED}},
            {TxState::AUTHORIZATION_REQUIRED, {TxState::AUTHORIZED, TxState::FAILED, TxState::CANCELLED}},
            {TxState::AUTHORIZED, {TxState::BACKUP_CREATED, TxState::FAILED, TxState::CANCELLED}},
            {TxState::BACKUP_CREATED, {TxState::APPLYING, TxState::FAILED}},
            {TxState::APPLYING, {TxState::APPLIED, TxState::FAILED}},
            {TxState::APPLIED, {TxState::VERIFYING, TxState::FAILED}},
            {TxState::VERIFYING, {TxState::VERIFIED, TxState::FAILED}},
            {TxState::VERIFIED, {TxState::COMPLETED, TxState::FAILED}},
            {TxState::FAILED, {TxState::ROLLING_BACK, TxState::CANCELLED}},
            {TxState::ROLLING_BACK, {TxState::ROLLED_BACK, TxState::FAILED}},
            // terminal
            {TxState::COMPLETED, {}},
            {TxState::ROLLED_BACK, {}},
            {TxState::CANCELLED, {}},
        };
        auto it = allowed.find(from);
        if(it==allowed.end()) return false;
        return std::find(it->second.begin(), it->second.end(), to)!=it->second.end();
    }

    static void validateTransition(TxState from, TxState to){
        if(!isValidTransition(from,to)){
            throw std::logic_error("Invalid transaction transition: " + toString(from) + " -> " + toString(to) + " - rejected, fail closed");
        }
    }

    static bool isTerminal(TxState s){
        return s==TxState::COMPLETED || s==TxState::ROLLED_BACK || s==TxState::CANCELLED;
    }
    static bool isFailed(TxState s){ return s==TxState::FAILED; }
};

} // namespace polaris::safety
