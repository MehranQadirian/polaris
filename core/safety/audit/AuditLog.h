#pragma once
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace polaris::safety {

struct AuditEvent {
    std::string timestamp;
    std::string transactionId;
    std::string operation; // e.g., "transaction.created", "backup.created", "authorization.denied"
    std::string user; // not password, just UID or session
    std::string approval;
    std::string authorizationResult;
    std::string backupPath;
    std::string changes;
    std::string verification;
    std::string rollback;
    std::string error;
    std::string previousHash;
    std::string eventHash; // SHA256 of this event + previousHash (tamper evident)
};

class AuditLog {
public:
    static void append(const AuditEvent& e);
    static std::vector<AuditEvent> list(const std::string& transactionId = "");
    static std::optional<AuditEvent> get(const std::string& eventHash);
    static std::string logPath(){
        const char* home = getenv("HOME");
        std::string base = home ? std::string(home)+"/.local/state/polaris/audit.log" : "/tmp/polaris-audit.log";
        return base;
    }
    static std::string testLogPath(){ return "/tmp/polaris-test-root/audit.log"; }

    static std::string hashEvent(const AuditEvent& e);
};

} // namespace polaris::safety
