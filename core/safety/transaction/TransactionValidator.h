#pragma once
#include "Transaction.h"
#include "../FileSafety.h"
#include "../backup/BackupEngine.h"
#include <string>
#include <map>
#include <optional>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

namespace polaris::safety {

struct CurrentState {
    std::string currentBeforeHash;           // sha256 of current target file content
    std::string currentUnitHash;             // sha256(enabled|active|package state) or service state hash
    std::string currentKernelVersion;        // uname -r
    std::string currentPackageStateHash;     // hash of relevant package list
    std::string currentTarget;               // current target path
    std::string currentOperation;            // current operationId
    std::map<std::string,std::string> currentPreconditions; // service.enabled, config.hash etc.
    std::string filePath;                    // for TOCTOU check (actual file path to re-validate)
    std::string currentCanonical;            // optional canonical path at observation time (for TOCTOU)
};

struct ValidationResult {
    bool valid = false;
    std::string reason;        // human readable
    std::string expected;      // expected value
    std::string observed;      // observed value
    std::string failingField;  // which field failed (target, beforeHash, unitHash, kernelVersion, packageState, precondition:<key>, state, approval, toctou.symlink, etc.)
    std::string auditOperation; // e.g., "validation.failed.stale_beforeHash"
};

class TransactionValidator {
public:
    // Compute sha256 hex of string content (same as sha256File but for string)
    static std::string hashString(const std::string& s){
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(s.c_str()), s.size(), hash);
        std::ostringstream oss;
        for(int i=0;i<SHA256_DIGEST_LENGTH;i++) oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        return oss.str();
    }

    // Bind approval - snapshot current state into approved* fields
    static void bindApproval(Transaction& tx, const CurrentState& curAtApproval){
        tx.approvedTarget = tx.target;
        tx.approvedOperation = tx.operationId;
        // Use current hash if provided, else fallback to preview hashes
        tx.approvedBeforeHash = curAtApproval.currentBeforeHash.empty() ? tx.beforeHash : curAtApproval.currentBeforeHash;
        tx.approvedUnitHash = curAtApproval.currentUnitHash.empty() ? tx.beforeUnitHash : curAtApproval.currentUnitHash;
        tx.approvedKernelVersion = curAtApproval.currentKernelVersion.empty() ? tx.kernelVersion : curAtApproval.currentKernelVersion;
        tx.approvedPackageStateHash = curAtApproval.currentPackageStateHash.empty() ? tx.packageStateHash : curAtApproval.currentPackageStateHash;
        tx.approvedPreconditions = curAtApproval.currentPreconditions.empty() ? tx.preconditions : curAtApproval.currentPreconditions;
        // Also copy current target/operation if empty in tx (defensive)
        if(tx.approvedTarget.empty()) tx.approvedTarget = curAtApproval.currentTarget;
        if(tx.approvedOperation.empty()) tx.approvedOperation = curAtApproval.currentOperation;
        tx.approvalState = "APPROVED";
        tx.idempotencyKey = tx.id;
    }

    static bool isIdempotentOperation(const std::string& operationId){
        // Explicit allowlist for idempotent operations (file writes with same content)
        // Non-idempotent: package installs, dnf swaps, akmods, dracut, systemctl enable/disable where state changes
        if(operationId.find("nvidia-settings")!=std::string::npos) return true; // Hidden=true toggle is idempotent if already true
        if(operationId.find("dummy-test")!=std::string::npos) return true; // test fixture idempotent
        if(operationId.find("fstab")!=std::string::npos) return true; // commenting same line twice is idempotent
        // All other real ops are non-idempotent by default
        return false;
    }

    static ValidationResult validateApprovalBinding(const Transaction& tx, const std::string& approvalId){
        ValidationResult r;
        r.valid = false;
        r.failingField = "approval";
        if(approvalId != tx.id){
            r.reason = "Approval bound to different transactionId: expected " + tx.id + " observed " + approvalId;
            r.expected = tx.id;
            r.observed = approvalId;
            r.auditOperation = "validation.failed.approval_mismatch";
            return r;
        }
        if(tx.approvalState != "APPROVED"){
            r.reason = "Transaction not in APPROVED state: " + tx.approvalState;
            r.expected = "APPROVED";
            r.observed = tx.approvalState;
            r.auditOperation = "validation.failed.not_approved";
            return r;
        }
        if(tx.approvedBeforeHash.empty()){
            r.reason = "missing approval binding (approvedBeforeHash empty) - new preview required";
            r.expected = "non-empty approvedBeforeHash";
            r.observed = "(empty)";
            r.failingField = "approvedBeforeHash";
            r.auditOperation = "validation.failed.missing_approval_binding";
            return r;
        }
        // approvedTarget/Operation must be present if tx has them
        if(!tx.approvedTarget.empty() && tx.approvedTarget != tx.target){
            // This is actually checked in stale, but we can flag here as inconsistency
        }
        r.valid = true;
        r.reason = "approval binding valid";
        r.failingField = "";
        r.auditOperation = "validation.passed.approval_binding";
        return r;
    }

    static bool isStale(const Transaction& tx, const CurrentState& cur){
        return !validateForApply(tx, cur).valid;
    }

    static ValidationResult validateForApply(const Transaction& tx, const CurrentState& cur){
        ValidationResult r;
        r.valid = true;

        // 1. Idempotency / terminal state
        if(tx.state == TxState::COMPLETED || tx.state == TxState::ROLLED_BACK || tx.state == TxState::CANCELLED){
            r.valid = false;
            r.reason = "transaction already terminal (" + toString(tx.state) + ") cannot re-apply - idempotent rejection";
            r.expected = "non-terminal state";
            r.observed = toString(tx.state);
            r.failingField = "state";
            r.auditOperation = "apply.rejected.already_completed";
            return r;
        }
        // 2. State machine gate: must be BACKUP_CREATED to go to APPLYING (or AUTHORIZED if backup not yet - but we enforce BACKUP_CREATED)
        // Allow also AUTHORIZED for first validation before backup; for final validation require BACKUP_CREATED
        // We'll enforce that final apply requires BACKUP_CREATED and backupState==CREATED
        // For generic stale check before backup, being in APPROVED/AUTHORIZATION_REQUIRED/AUTHORIZED still valid.
        // So we only reject if state is COMPLETELY wrong like PROPOSED/PREVIEWED/APPROVAL_REQUIRED/FORWARD terminal
        if(tx.state == TxState::PROPOSED || tx.state == TxState::PREVIEWED || tx.state == TxState::APPROVAL_REQUIRED){
            r.valid = false;
            r.reason = "invalid state for apply: " + toString(tx.state) + " - must be APPROVED/AUTHORIZED/BACKUP_CREATED";
            r.expected = "APPROVED or AUTHORIZED or BACKUP_CREATED";
            r.observed = toString(tx.state);
            r.failingField = "state";
            r.auditOperation = "validation.failed.invalid_state_for_apply";
            return r;
        }

        // 3. Approval must be APPROVED
        if(tx.approvalState != "APPROVED"){
            r.valid = false;
            r.reason = "transaction not approved: approvalState=" + tx.approvalState;
            r.expected = "APPROVED";
            r.observed = tx.approvalState;
            r.failingField = "approvalState";
            r.auditOperation = "validation.failed.not_approved";
            return r;
        }

        // 4. Approval binding must exist
        if(tx.approvedBeforeHash.empty()){
            r.valid = false;
            r.reason = "missing approval binding (approvedBeforeHash empty) - new preview required";
            r.expected = "non-empty approvedBeforeHash";
            r.observed = "(empty)";
            r.failingField = "approvedBeforeHash";
            r.auditOperation = "validation.failed.missing_approval_binding";
            return r;
        }

        // 5. Target must match approvedTarget
        if(!tx.approvedTarget.empty() && cur.currentTarget != tx.approvedTarget){
            // also compare tx.target vs approvedTarget in case tx.target drifted without cur
            r.valid = false;
            r.reason = "stale target: expected " + tx.approvedTarget + " observed " + cur.currentTarget;
            r.expected = tx.approvedTarget;
            r.observed = cur.currentTarget;
            r.failingField = "target";
            r.auditOperation = "validation.failed.stale_target";
            return r;
        }
        // Also check tx.target vs approvedTarget (in case CurrentState not provided but tx mutated)
        if(!tx.approvedTarget.empty() && tx.target != tx.approvedTarget){
            r.valid = false;
            r.reason = "stale target (transaction mutated): expected " + tx.approvedTarget + " observed " + tx.target;
            r.expected = tx.approvedTarget;
            r.observed = tx.target;
            r.failingField = "target";
            r.auditOperation = "validation.failed.stale_target";
            return r;
        }

        // 6. Operation must match
        if(!tx.approvedOperation.empty() && cur.currentOperation != tx.approvedOperation){
            r.valid = false;
            r.reason = "stale operation: expected " + tx.approvedOperation + " observed " + cur.currentOperation;
            r.expected = tx.approvedOperation;
            r.observed = cur.currentOperation;
            r.failingField = "operation";
            r.auditOperation = "validation.failed.stale_operation";
            return r;
        }
        if(!tx.approvedOperation.empty() && tx.operationId != tx.approvedOperation){
            r.valid = false;
            r.reason = "stale operation (transaction mutated): expected " + tx.approvedOperation + " observed " + tx.operationId;
            r.expected = tx.approvedOperation;
            r.observed = tx.operationId;
            r.failingField = "operation";
            r.auditOperation = "validation.failed.stale_operation";
            return r;
        }

        // 7. beforeHash
        if(!tx.approvedBeforeHash.empty() && !cur.currentBeforeHash.empty() && cur.currentBeforeHash != tx.approvedBeforeHash){
            r.valid = false;
            r.reason = "stale beforeHash: expected " + tx.approvedBeforeHash.substr(0,16) + "... observed " + cur.currentBeforeHash.substr(0,16) + "...";
            r.expected = tx.approvedBeforeHash;
            r.observed = cur.currentBeforeHash;
            r.failingField = "beforeHash";
            r.auditOperation = "validation.failed.stale_beforeHash";
            return r;
        }
        if(!tx.approvedBeforeHash.empty() && cur.currentBeforeHash.empty()){
            r.valid = false;
            r.reason = "unverifiable beforeHash: expected " + tx.approvedBeforeHash.substr(0,16) + "... observed empty (unavailable)";
            r.expected = tx.approvedBeforeHash;
            r.observed = "(unavailable)";
            r.failingField = "beforeHash";
            r.auditOperation = "validation.failed.unverifiable_beforeHash";
            return r;
        }
        // If currentBeforeHash empty but we have filePath, try to compute? Instead fail closed if approved exists but current cannot be observed (already handled above)
        if(!tx.approvedBeforeHash.empty() && cur.currentBeforeHash.empty() && !cur.filePath.empty()){
            // Attempts to re-derive? If file doesn't exist or unreadable, fail closed (already returned unverifiable above)
            try {
                if(!FileSafety::isRegularFile(cur.filePath)){
                    r.valid = false;
                    r.reason = "unverifiable beforeHash: target file not regular or missing: " + cur.filePath;
                    r.expected = tx.approvedBeforeHash;
                    r.observed = "(unverifiable)";
                    r.failingField = "beforeHash";
                    r.auditOperation = "validation.failed.unverifiable_beforeHash";
                    return r;
                }
            } catch(...){
                r.valid = false;
                r.reason = "unverifiable beforeHash: exception checking file";
                r.expected = tx.approvedBeforeHash;
                r.observed = "(exception)";
                r.failingField = "beforeHash";
                r.auditOperation = "validation.failed.unverifiable_beforeHash";
                return r;
            }
        }

        // 8. unitHash
        if(!tx.approvedUnitHash.empty() && !cur.currentUnitHash.empty() && cur.currentUnitHash != tx.approvedUnitHash){
            r.valid = false;
            r.reason = "stale unitHash: expected " + tx.approvedUnitHash.substr(0,16) + "... observed " + cur.currentUnitHash.substr(0,16) + "...";
            r.expected = tx.approvedUnitHash;
            r.observed = cur.currentUnitHash;
            r.failingField = "unitHash";
            r.auditOperation = "validation.failed.stale_unitHash";
            return r;
        }
        if(!tx.approvedUnitHash.empty() && cur.currentUnitHash.empty()){
            r.valid = false;
            r.reason = "unverifiable unitHash: expected " + tx.approvedUnitHash.substr(0,16) + "... observed empty";
            r.expected = tx.approvedUnitHash;
            r.observed = "(unverifiable)";
            r.failingField = "unitHash";
            r.auditOperation = "validation.failed.unverifiable_unitHash";
            return r;
        }

        // 9. kernelVersion
        if(!tx.approvedKernelVersion.empty() && !cur.currentKernelVersion.empty() && cur.currentKernelVersion != tx.approvedKernelVersion){
            r.valid = false;
            r.reason = "stale kernelVersion: expected " + tx.approvedKernelVersion + " observed " + cur.currentKernelVersion;
            r.expected = tx.approvedKernelVersion;
            r.observed = cur.currentKernelVersion;
            r.failingField = "kernelVersion";
            r.auditOperation = "validation.failed.stale_kernelVersion";
            return r;
        }
        if(!tx.approvedKernelVersion.empty() && cur.currentKernelVersion.empty()){
            r.valid = false;
            r.reason = "unverifiable kernelVersion: expected " + tx.approvedKernelVersion + " observed empty (unavailable)";
            r.expected = tx.approvedKernelVersion;
            r.observed = "(unavailable)";
            r.failingField = "kernelVersion";
            r.auditOperation = "validation.failed.unverifiable_kernelVersion";
            return r;
        }

        // 10. packageStateHash
        if(!tx.approvedPackageStateHash.empty() && !cur.currentPackageStateHash.empty() && cur.currentPackageStateHash != tx.approvedPackageStateHash){
            r.valid = false;
            r.reason = "stale packageStateHash: expected " + tx.approvedPackageStateHash.substr(0,16) + "... observed " + cur.currentPackageStateHash.substr(0,16) + "...";
            r.expected = tx.approvedPackageStateHash;
            r.observed = cur.currentPackageStateHash;
            r.failingField = "packageState";
            r.auditOperation = "validation.failed.stale_packageState";
            return r;
        }
        if(!tx.approvedPackageStateHash.empty() && cur.currentPackageStateHash.empty()){
            r.valid = false;
            r.reason = "unverifiable packageStateHash: expected " + tx.approvedPackageStateHash.substr(0,16) + "... observed empty (unavailable)";
            r.expected = tx.approvedPackageStateHash;
            r.observed = "(unavailable)";
            r.failingField = "packageState";
            r.auditOperation = "validation.failed.unverifiable_packageState";
            return r;
        }

        // 11. generic preconditions
        for(auto &kv : tx.approvedPreconditions){
            auto it = cur.currentPreconditions.find(kv.first);
            if(it == cur.currentPreconditions.end()){
                r.valid = false;
                r.reason = "unverifiable precondition: " + kv.first + " missing in current state";
                r.expected = kv.second;
                r.observed = "(missing)";
                r.failingField = "precondition:" + kv.first;
                r.auditOperation = "validation.failed.stale_precondition";
                return r;
            }
            if(it->second != kv.second){
                r.valid = false;
                r.reason = "stale precondition " + kv.first + ": expected " + kv.second + " observed " + it->second;
                r.expected = kv.second;
                r.observed = it->second;
                r.failingField = "precondition:" + kv.first;
                r.auditOperation = "validation.failed.stale_precondition";
                return r;
            }
        }

        // 12. TOCTOU checks if filePath provided
        if(!cur.filePath.empty()){
            // symlink check
            if(FileSafety::isSymlink(cur.filePath)){
                r.valid = false;
                r.reason = "TOCTOU symlink detected: " + cur.filePath + " is now a symlink";
                r.expected = "regular file";
                r.observed = "symlink";
                r.failingField = "toctou.symlink";
                r.auditOperation = "validation.failed.toctou_symlink";
                return r;
            }
            // canonical check if provided
            if(!cur.currentCanonical.empty()){
                try {
                    std::string nowCanon = FileSafety::canonical(cur.filePath);
                    if(nowCanon != cur.currentCanonical){
                        r.valid = false;
                        r.reason = "TOCTOU canonical drift: expected " + cur.currentCanonical + " observed " + nowCanon;
                        r.expected = cur.currentCanonical;
                        r.observed = nowCanon;
                        r.failingField = "toctou.canonical";
                        r.auditOperation = "validation.failed.toctou_canonical";
                        return r;
                    }
                } catch(...){
                    r.valid = false;
                    r.reason = "TOCTOU canonical unverifiable: " + cur.filePath;
                    r.expected = cur.currentCanonical;
                    r.observed = "(unverifiable)";
                    r.failingField = "toctou.canonical";
                    r.auditOperation = "validation.failed.toctou_canonical_unverifiable";
                    return r;
                }
            }
        }

        r.valid = true;
        r.reason = "all preconditions valid";
        r.expected = "";
        r.observed = "";
        r.failingField = "";
        r.auditOperation = "validation.passed";
        return r;
    }

    static ValidationResult finalPreconditionValidation(const Transaction& tx, const CurrentState& curAfterBackup){
        // Same as validateForApply but also checks backupState==CREATED and file still not stale after backup
        ValidationResult r = validateForApply(tx, curAfterBackup);
        if(!r.valid) {
            r.auditOperation = "validation.failed.final_" + r.failingField;
            if(r.auditOperation=="validation.failed.final_") r.auditOperation="validation.failed.final_precondition";
            return r;
        }
        if(tx.backupState != "CREATED"){
            r.valid = false;
            r.reason = "backup not created: backupState=" + tx.backupState;
            r.expected = "CREATED";
            r.observed = tx.backupState;
            r.failingField = "backupState";
            r.auditOperation = "validation.failed.backup_not_created";
            return r;
        }
        // Additional: ensure backup still exists and not overwritten? But that's BackupEngine's guarantee.
        r.valid = true;
        r.reason = "final precondition validation passed";
        r.auditOperation = "validation.passed.final";
        return r;
    }
};

} // namespace polaris::safety
