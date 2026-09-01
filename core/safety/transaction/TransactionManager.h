#pragma once
#include "../../domain/Transaction.h"
#include <string>
#include <optional>

namespace polaris::safety {

// Mandatory gate for all mutates. No direct /etc/systemd/dnf bypass.

class TransactionManager {
public:
    // 11-step safety: detect, validate prereq, compat, conflicts, risk, reversibility, backup, tx, approve, auth, apply, verify, benchmark, compare, keep/rollback
    domain::Transaction create(const std::vector<std::string>& optimizationIds, bool dryRun=false);
    bool validate(const domain::Transaction& tx);
    bool backup(const domain::Transaction& tx);
    bool apply(const domain::Transaction& tx); // calls privileged helper via Polkit, only after approve+auth
    bool verify(const domain::Transaction& tx);
    bool rollback(const std::string& txId);

    std::optional<domain::Transaction> get(const std::string& id) const;
};

} // namespace polaris::safety
