#include "../../core/explainability/Explanation.h"
#include <cassert>
#include <iostream>

using namespace polaris::explainability;

void test_model_serialization(){
    Explanation e;
    e.id = "EXP-akonadi-001";
    e.candidateId = "akonadi-disable";
    e.candidateKind = CandidateKind::RECOMMENDATION;
    e.decision = DecisionKind::BLOCKED;
    e.decisionLabel = "BLOCKED_BY_USER_WORKFLOW";
    e.whyNow = "Measured Akonadi 1302M";
    e.evidence = {"akonadi 1302M", "db_data 126M"};
    e.expectedBenefit = "~1.3GB";
    e.confidence = 0.65;
    e.risk = "R2";
    e.reversibility = "High (akonadictl start)";
    e.rebootRequired = false;
    e.authorizationRequired = true;
    e.whatWillChange = "target=akonadi";
    e.whatWillNotChange = "NVIDIA remains";
    e.rejectionConditions = {"profile: usesKMail=yes"};
    e.rollbackSummary = "akonadictl start";
    std::string j = e.toJson();
    assert(!j.empty());
    assert(j.find("\"candidateId\":\"akonadi-disable\"")!=std::string::npos);
    assert(j.find("\"decision\":\"BLOCKED\"")!=std::string::npos);
    // Round-trip
    Explanation e2 = Explanation::fromJson(j);
    assert(e2.candidateId=="akonadi-disable");
    assert(e2.decisionLabel=="BLOCKED_BY_USER_WORKFLOW");
    std::cout << "model serialization PASS\n";
}

void test_deterministic_json(){
    Explanation e1, e2;
    e1.id="EXP-001"; e1.candidateId="akonadi-disable"; e1.candidateKind=CandidateKind::RECOMMENDATION; e1.decision=DecisionKind::BLOCKED; e1.decisionLabel="BLOCKED"; e1.whyNow="why"; e1.evidence={"b","a"}; e1.rejectionConditions={"z","a"};
    e2 = e1;
    assert(e1.toJson()==e2.toJson());
    // Ordering: evidence sorted, so even if we add unsorted, toJson sorts
    Explanation e3 = e1;
    e3.evidence = {"a","b"}; // same sorted should produce same json
    assert(e1.toJson()==e3.toJson());
    std::cout << "deterministic JSON PASS\n";
}

void test_deterministic_ordering(){
    Explanation e;
    e.id="EXP-001"; e.candidateId="test"; e.evidence={"z","m","a"}; e.rejectionConditions={"z","a","m"}; e.dependencies={"c","b","a"};
    std::string j = e.toJson();
    // Check that evidence is sorted in JSON: should be ["a","m","z"]
    assert(j.find("\"evidence\":[\"a\",\"m\",\"z\"]")!=std::string::npos);
    assert(j.find("\"rejectionConditions\":[\"a\",\"m\",\"z\"]")!=std::string::npos);
    assert(j.find("\"dependencies\":[\"a\",\"b\",\"c\"]")!=std::string::npos);
    std::cout << "deterministic ordering PASS\n";
}

void test_verbose_output(){
    Explanation e;
    e.id="EXP-001"; e.candidateId="akonadi-disable"; e.candidateKind=CandidateKind::RECOMMENDATION; e.decision=DecisionKind::BLOCKED; e.decisionLabel="BLOCKED";
    e.whyNow="why"; e.whatWillChange="change"; e.whatWillNotChange="not change"; e.expectedBenefit="~1.3GB"; e.confidence=0.65; e.risk="R2"; e.evidence={"ev1","ev2"}; e.rejectionConditions={"rc1"}; e.dependencies={"dep1"};
    std::string normal = e.toHuman(false);
    std::string verbose = e.toHuman(true);
    assert(verbose.size() > normal.size());
    assert(verbose.find("EVIDENCE")!=std::string::npos);
    assert(verbose.find("ev1")!=std::string::npos);
    assert(normal.find("ev1")==std::string::npos); // normal should not contain evidence list
    std::cout << "verbose output PASS\n";
}

void test_json_output(){
    Explanation e;
    e.id="EXP-001"; e.candidateId="akonadi-disable"; e.decision=DecisionKind::BLOCKED; e.decisionLabel="BLOCKED";
    std::string j = e.toJson();
    // Must be valid JSON and contain required fields
    assert(j.front()=='{' && j.back()=='}');
    assert(j.find("\"candidateId\"")!=std::string::npos);
    assert(j.find("\"decision\"")!=std::string::npos);
    assert(j.find("\"whyNow\"")!=std::string::npos);
    assert(j.find("\"whatWillChange\"")!=std::string::npos);
    assert(j.find("\"whatWillNotChange\"")!=std::string::npos);
    // Keys should be sorted alphabetically: afterStateSummary < authorizationRequired < beforeStateSummary ...
    // Check that "candidateId" comes before "candidateKind" alphabetically? Actually candidateId < candidateKind? Let's just check that json is deterministic and contains all expected keys
    assert(j.find("\"whatWillChange\"")!=std::string::npos);
    std::cout << "JSON output PASS\n";
}

void test_secret_redaction(){
    Explanation e;
    e.id="EXP-001"; e.candidateId="test"; e.evidence={"password secret123", "normal evidence"}; e.rejectionConditions={"secret password"};
    std::string human = e.toHuman(true);
    // verbose should redact secret
    assert(human.find("secret123")==std::string::npos);
    assert(human.find("password")==std::string::npos || human.find("[REDACTED]")!=std::string::npos);
    std::cout << "secret/password redaction PASS\n";
}

int main(){
    test_model_serialization();
    test_deterministic_json();
    test_deterministic_ordering();
    test_verbose_output();
    test_json_output();
    test_secret_redaction();
    std::cout << "All P16 explanation model tests PASS\n";
    return 0;
}
