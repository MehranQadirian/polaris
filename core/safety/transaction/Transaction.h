#pragma once
#include "StateMachine.h"
#include "../../domain/PerfModels.h"
#include "../../domain/Comparison.h"
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <map>

namespace polaris::safety {

struct ChangePreview {
    std::string target; // e.g., "/etc/fstab" or "/tmp/polaris-test-root/etc/fstab"
    std::string beforeState; // file content hash or service status
    std::string afterState; // expected content
    std::string diff; // unified diff human-readable
    std::string method; // "atomic write via helper FileModify"
    std::string privilege; // "org.polaris.modify.fstab"
    std::string risk; // R0-3
    std::string benefit;
    std::string rollback;
    bool rebootRequired=false;
};

struct Transaction {
    std::string id; // TX-2026-000001
    std::string operationId; // e.g., "fstab-stale-swap" or "dummy-test"
    std::string target;
    std::string description;
    std::string riskLevel; // R0-3
    std::string expectedBenefit;
    std::string observedBenefit; // P11: from Comparison
    std::string requiredPrivileges; // polkit action
    TxState state = TxState::PROPOSED;
    std::string approvalState; // "PENDING","APPROVED","REJECTED"
    std::string authorizationState; // "PENDING","GRANTED","DENIED"
    std::string backupState; // "NONE","CREATED","FAILED"
    std::string executionState; // "PENDING","APPLYING","APPLIED","FAILED"
    std::string verificationState; // "PENDING","PASSED","FAILED"
    std::string rollbackState; // "NONE","AVAILABLE","EXECUTED"
    std::string beforeState;
    std::string afterState;
    std::vector<ChangePreview> previews;
    std::string evidence;
    std::string rollbackPlan;
    bool rebootRequired=false;
    std::string status; // human
    std::string error;
    std::string auditReference;
    std::string timestamp;
    // hash chaining
    std::string previousHash;
    std::string eventHash;
    // P11: post-change measurement (backward compatible: optional)
    std::optional<domain::PerformanceBaseline> beforeBaseline;
    std::optional<domain::PerformanceBaseline> afterBaseline;
    std::optional<domain::Comparison> comparison;

    // P12: stale-preview protection + idempotency (backward compatible: empty = not set)
    std::string beforeHash;                // sha256(beforeState) at preview
    std::string approvedBeforeHash;        // sha256 at approval (binding)
    std::string beforeUnitHash;            // hash(enabled|active|package) at preview
    std::string approvedUnitHash;          // at approval
    std::string kernelVersion;             // uname -r at preview
    std::string approvedKernelVersion;     // at approval
    std::string packageStateHash;          // hash of relevant package list at preview
    std::string approvedPackageStateHash;  // at approval
    std::string approvedTarget;            // target copy at approval
    std::string approvedOperation;         // operationId copy at approval
    std::map<std::string,std::string> preconditions;          // snapshot at preview
    std::map<std::string,std::string> approvedPreconditions;  // at approval
    std::string idempotencyKey;            // == id, explicit
    std::string appliedAt;                 // ISO8601 when APPLY succeeded
    std::string completedAt;               // ISO8601 when COMPLETED
    std::string validationResult;          // last rejection reason
};

} // namespace polaris::safety
