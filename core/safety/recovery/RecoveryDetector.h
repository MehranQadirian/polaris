#pragma once
#include "../../safety/transaction/Transaction.h"
#include "../../safety/transaction/StateMachine.h"
#include <string>
#include <vector>
#include <filesystem>

namespace polaris::safety {

struct RecoveryInfo {
    std::string id;
    TxState state = TxState::PROPOSED;
    std::string backupPath;
    bool backupExists = false;
    TxState suggested = TxState::FAILED; // fail-closed: suggest FAILED, not COMPLETED
    std::string reason;
};

class RecoveryDetector {
public:
    // Scan transaction store directory (real or test) for incomplete transactions
    // Returns vector of RecoveryInfo for states that are incomplete and need attention
    static std::vector<RecoveryInfo> detect(const std::string& storePath = defaultStorePath());

    static std::string defaultStorePath(){
        const char* home = getenv("HOME");
        std::string base = home ? std::string(home)+"/.local/state/polaris/transactions" : "/tmp/polaris-transactions";
        return base;
    }
    static std::string testStorePath(){ return "/tmp/polaris-test-root/transactions"; }
    static std::string testStorePath(const std::string& dir){
        if(dir.empty()) return testStorePath();
        if(dir.back()=='/') return dir + "transactions";
        return dir + "/transactions";
    }

    // Check if a state is considered incomplete (needs recovery)
    static bool isIncomplete(TxState s);

    // Check if recovery should fail closed (never auto-apply)
    static bool shouldFailClosed(const RecoveryInfo& info){ (void)info; return true; }

private:
    static std::string nowISO();
    static void audit(const std::string& op, const std::string& detail, const std::string& txId);
    static TxState parseState(const std::string& stateStr);
};

} // namespace polaris::safety
