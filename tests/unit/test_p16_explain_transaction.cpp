#include "../../core/explainability/ExplanationEngine.h"
#include "../../core/profile/UserProfile.h"
#include "../../core/safety/transaction/Transaction.h"
#include "../../core/safety/transaction/TransactionValidator.h"
#include "../../core/domain/Comparison.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

using namespace polaris::explainability;
using namespace polaris::profile;
using namespace polaris::safety;
using namespace polaris::domain;

void test_expected_vs_observed(){
    Transaction tx;
    tx.id = "TX-TEST-P16-EXPECTED-001";
    tx.operationId = "akonadi-disable";
    tx.target = "akonadi";
    tx.expectedBenefit = "~1.3GB RAM";
    tx.state = TxState::COMPLETED;
    UserProfile p;
    Comparison cmp;
    cmp.expectedBenefit = "~1.3GB RAM";
    cmp.observedBenefit = "no benefit, akonadi still running";
    cmp.verdict = Verdict::NO_BENEFIT;
    cmp.verdictReason = "Operation succeeded but no benefit observed";
    cmp.hasRegression = false;
    cmp.beforeBaseline.systemd.userspace = 8.515;
    cmp.afterBaseline.systemd.userspace = 8.515;
    auto exp = ExplanationEngine::explainTransaction(tx, p, &cmp, nullptr);
    assert(exp.expectedBenefit=="~1.3GB RAM");
    assert(exp.observedBenefit.find("no benefit")!=std::string::npos || exp.observedBenefit.find("no benefit")!=std::string::npos);
    assert(exp.verdict=="NO_BENEFIT");
    assert(exp.whyNow.find("akonadi")!=std::string::npos || !exp.whyNow.empty());
    std::cout << "expected vs observed benefit PASS (verdict " << exp.verdict << ")\n";
}

void test_regression_explanation(){
    Transaction tx;
    tx.id = "TX-TEST-P16-REGRESSION";
    tx.state = TxState::COMPLETED;
    tx.expectedBenefit = "faster boot";
    UserProfile p;
    Comparison cmp;
    cmp.verdict = Verdict::REGRESSION;
    cmp.verdictReason = "boot +40% >10% threshold";
    cmp.hasRegression = true;
    cmp.observedBenefit = "regression detected";
    MetricComparison m;
    m.metric="boot.userspace"; m.before=50; m.after=70; m.delta=20; m.pctDelta=40; m.thresholdDesc="boot > +10% relative"; m.thresholdValue=10; m.thresholdType="relative_pct"; m.regression=true;
    cmp.metrics.push_back(m);
    auto exp = ExplanationEngine::explainTransaction(tx, p, &cmp, nullptr);
    assert(exp.hasRegression);
    assert(exp.verdict=="REGRESSION");
    bool found=false;
    for(auto &rc: exp.rejectionConditions) if(rc.find("regression")!=std::string::npos) found=true;
    assert(found);
    std::cout << "regression explanation PASS\n";
}

void test_failed_explanation(){
    Transaction tx;
    tx.id = "TX-TEST-P16-FAILED";
    tx.state = TxState::FAILED;
    tx.error = "stale beforeHash: expected abc observed def";
    tx.validationResult = "stale beforeHash";
    tx.approvalState = "APPROVED";
    tx.backupState = "CREATED";
    UserProfile p;
    ValidationResult vr;
    vr.valid=false; vr.reason="stale beforeHash"; vr.expected="abc"; vr.observed="def"; vr.failingField="beforeHash"; vr.auditOperation="validation.failed.stale_beforeHash";
    auto exp = ExplanationEngine::explainTransaction(tx, p, nullptr, &vr);
    assert(exp.decision==DecisionKind::FAILED);
    assert(exp.decisionLabel.find("FAILED")!=std::string::npos);
    assert(exp.whyNow.find("FAILED")!=std::string::npos || exp.whyNow.find("stale")!=std::string::npos || !exp.whyNow.empty());
    bool found=false;
    for(auto &rc: exp.rejectionConditions) if(rc.find("beforeHash")!=std::string::npos) found=true;
    assert(found);
    assert(exp.rollbackSummary.find("backup")!=std::string::npos || exp.rollbackSummary.find("Restore")!=std::string::npos);
    std::cout << "transaction FAILED explanation PASS\n";
}

void test_rollback_explanation(){
    Transaction tx;
    tx.id = "TX-TEST-P16-ROLLBACK";
    tx.state = TxState::BACKUP_CREATED;
    tx.rollbackPlan = "Restore from backup /tmp/polaris-test-root/backups/TX-TEST/bak, systemctl enable akonadi";
    tx.target = "/tmp/polaris-test-root/etc/fstab";
    UserProfile p;
    auto exp = ExplanationEngine::explainTransaction(tx, p, nullptr, nullptr);
    assert(!exp.rollbackSummary.empty());
    assert(exp.rollbackSummary.find("backup")!=std::string::npos || exp.rollbackSummary.find("akonadi")!=std::string::npos);
    std::cout << "rollback explanation PASS\n";
}

void test_completed_explanation(){
    Transaction tx;
    tx.id = "TX-TEST-P16-COMPLETED";
    tx.state = TxState::COMPLETED;
    tx.expectedBenefit = "faster boot";
    UserProfile p;
    Comparison cmp;
    cmp.verdict = Verdict::SUCCESS;
    cmp.observedBenefit = "MX130 claimed, success";
    cmp.verdictReason = "Observed benefit matches expected";
    cmp.hasRegression=false;
    auto exp = ExplanationEngine::explainTransaction(tx, p, &cmp, nullptr);
    assert(exp.decision==DecisionKind::COMPLETED);
    assert(exp.verdict=="SUCCESS");
    assert(exp.observedBenefit.find("MX130")!=std::string::npos);
    std::cout << "completed transaction explanation PASS\n";
    // Without comparison, limitations should indicate unavailable
    Transaction tx2;
    tx2.id = "TX-TEST-P16-COMPLETED-NO-CMP";
    tx2.state = TxState::COMPLETED;
    auto exp2 = ExplanationEngine::explainTransaction(tx2, p, nullptr, nullptr);
    assert(!exp2.limitations.empty());
    assert(exp2.limitations.find("unavailable")!=std::string::npos || exp2.limitations.find("Comparison")!=std::string::npos);
    std::cout << "completed without comparison limitations PASS\n";
}

void test_stale_preview_explanation(){
    Transaction tx;
    tx.id = "TX-TEST-P16-STALE";
    tx.state = TxState::FAILED;
    tx.validationResult = "stale beforeHash";
    UserProfile p;
    ValidationResult vr;
    vr.valid=false; vr.expected="abc123"; vr.observed="def456"; vr.failingField="beforeHash"; vr.reason="stale beforeHash";
    auto exp = ExplanationEngine::explainTransaction(tx, p, nullptr, &vr);
    assert(exp.decision==DecisionKind::FAILED);
    bool found=false;
    for(auto &rc: exp.rejectionConditions) if(rc.find("beforeHash")!=std::string::npos && rc.find("abc123")!=std::string::npos) found=true;
    assert(found);
    std::cout << "stale-preview explanation PASS\n";
}

void test_authorization_distinction(){
    Transaction tx;
    tx.id = "TX-TEST-P16-AUTH";
    tx.state = TxState::APPROVAL_REQUIRED;
    tx.approvalState = "PENDING";
    tx.requiredPrivileges = "org.polaris.modify.fstab";
    UserProfile p;
    auto exp = ExplanationEngine::explainTransaction(tx, p, nullptr, nullptr);
    assert(exp.authorizationRequired==true);
    assert(exp.decisionLabel.find("APPROVAL_REQUIRED")!=std::string::npos || exp.decision==DecisionKind::PREVIEWED);
    // Explain should not confuse approval with authorization
    assert(exp.whyNow.find("approval")!=std::string::npos || exp.rejectionConditions.size()>0);
    std::cout << "authorization distinction PASS\n";
}

void test_no_mutation_during_explanation(){
    std::string dir = "/tmp/polaris-test-root/p16_no_mutation";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string file = dir + "/fstab";
    { std::ofstream out(file); out << "original\n"; }
    struct stat st1; ::stat(file.c_str(), &st1);
    Transaction tx;
    tx.id = "TX-TEST-P16-NO-MUT";
    tx.target = file;
    tx.state = TxState::PREVIEWED;
    UserProfile p;
    auto exp = ExplanationEngine::explainTransaction(tx, p, nullptr, nullptr);
    (void)exp;
    struct stat st2; ::stat(file.c_str(), &st2);
    assert(st1.st_mtime==st2.st_mtime);
    std::string content;
    { std::ifstream f(file); content.assign(std::istreambuf_iterator<char>(f), {}); }
    assert(content=="original\n");
    std::cout << "no mutation during explanation PASS\n";
}

int main(){
    test_expected_vs_observed();
    test_regression_explanation();
    test_failed_explanation();
    test_rollback_explanation();
    test_completed_explanation();
    test_stale_preview_explanation();
    test_authorization_distinction();
    test_no_mutation_during_explanation();
    std::cout << "All P16 explain transaction tests PASS\n";
    return 0;
}
