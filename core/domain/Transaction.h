#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <optional>

namespace polaris::domain {

enum class Risk { R0=0, R1=1, R2=2, R3=3, R4=4 };
enum class State { PLANNED, VALIDATING, BACKING_UP, WAITING_FOR_APPROVAL,
                   WAITING_FOR_AUTHORIZATION, APPLYING, VERIFYING,
                   BENCHMARKING, COMPLETED, FAILED, ROLLING_BACK, ROLLED_BACK, REQUIRES_USER_ACTION };

struct Change {
    std::string id;
    std::string file;
    std::string oldHash, newContent;
    std::optional<std::string> backupId;
};

struct Backup {
    std::string id;
    std::string txId;
    std::string path;
    std::string sha256;
    std::string storedAt;
    std::string reason;
};

struct Optimization {
    std::string id;
    std::string title;
    std::string category;
    std::string problem;
    std::vector<std::string> evidence;
    std::string expectedBenefit;
    Risk risk = Risk::R0;
    bool requiresReboot=false, requiresAuth=false;
    bool rollbackAvailable=true;
    std::vector<Change> actions;
};

struct Transaction {
    std::string id; // TX-2026-000001
    State state = State::PLANNED;
    std::vector<std::string> optimizationIds;
    Risk risk = Risk::R0;
    std::vector<Change> changes;
    std::optional<Backup> backup;
    std::string approvalBy;
    std::string authAction; // polkit action
    std::chrono::system_clock::time_point created;
};

} // namespace polaris::domain
