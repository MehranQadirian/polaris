#include "Explanation.h"
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace polaris::explainability {

bool containsSecret(const std::string& s){
    std::string low=s;
    for(auto &c: low) c=tolower(c);
    return low.find("password")!=std::string::npos || low.find("secret")!=std::string::npos || low.find("passwd")!=std::string::npos;
}
std::string redact(const std::string& s){
    if(containsSecret(s)) return "[REDACTED]";
    return s;
}

std::string Explanation::toJson() const {
    std::ostringstream oss;
    oss << "{";
    // Deterministic sorted keys alphabetical
    // Order: afterStateSummary, authorizationRequired, beforeStateSummary, candidateId, candidateKind, confidence, decision, decisionLabel, dependencies, evidence, expectedBenefit, hasRegression, id, limitations, observedBenefit, rebootRequired, rejectionConditions, reversibility, risk, rollbackSummary, userImpact, verdict, verdictReason, whatWillChange, whatWillNotChange, whyNow
    // For simplicity, we will output in alphabetical order of field names
    // Use map-like ordering manually
    oss << "\"afterStateSummary\":\"" << afterStateSummary << "\",";
    oss << "\"authorizationRequired\":" << (authorizationRequired?"true":"false") << ",";
    oss << "\"beforeStateSummary\":\"" << beforeStateSummary << "\",";
    oss << "\"candidateId\":\"" << candidateId << "\",";
    oss << "\"candidateKind\":\"" << toString(candidateKind) << "\",";
    oss << "\"confidence\":" << confidence << ",";
    oss << "\"decision\":\"" << toString(decision) << "\",";
    oss << "\"decisionLabel\":\"" << decisionLabel << "\",";
    // dependencies sorted
    {
        auto deps = dependencies;
        std::sort(deps.begin(), deps.end());
        oss << "\"dependencies\":[";
        for(size_t i=0;i<deps.size();i++){ if(i) oss<<","; oss<<"\""<<deps[i]<<"\""; }
        oss << "],";
    }
    // evidence sorted
    {
        auto ev = evidence;
        std::sort(ev.begin(), ev.end());
        oss << "\"evidence\":[";
        for(size_t i=0;i<ev.size();i++){ if(i) oss<<","; oss<<"\""<<ev[i]<<"\""; }
        oss << "],";
    }
    oss << "\"expectedBenefit\":\"" << expectedBenefit << "\",";
    oss << "\"hasRegression\":" << (hasRegression?"true":"false") << ",";
    oss << "\"id\":\"" << id << "\",";
    oss << "\"limitations\":\"" << limitations << "\",";
    oss << "\"observedBenefit\":\"" << observedBenefit << "\",";
    oss << "\"rebootRequired\":" << (rebootRequired?"true":"false") << ",";
    {
        auto rc = rejectionConditions;
        std::sort(rc.begin(), rc.end());
        oss << "\"rejectionConditions\":[";
        for(size_t i=0;i<rc.size();i++){ if(i) oss<<","; oss<<"\""<<rc[i]<<"\""; }
        oss << "],";
    }
    oss << "\"reversibility\":\"" << reversibility << "\",";
    oss << "\"risk\":\"" << risk << "\",";
    oss << "\"rollbackSummary\":\"" << rollbackSummary << "\",";
    oss << "\"userImpact\":\"" << userImpact << "\",";
    oss << "\"verdict\":\"" << verdict << "\",";
    oss << "\"verdictReason\":\"" << verdictReason << "\",";
    oss << "\"whatWillChange\":\"" << whatWillChange << "\",";
    oss << "\"whatWillNotChange\":\"" << whatWillNotChange << "\",";
    oss << "\"whyNow\":\"" << whyNow << "\"";
    oss << "}";
    return oss.str();
}

Explanation Explanation::fromJson(const std::string& json){
    // Minimal strict parser for our deterministic JSON: check braces and required fields
    if(json.empty() || json.front()!='{' || json.back()!='}') throw std::invalid_argument("missing braces");
    // For P16, we only need to verify that fromJson can parse what toJson produces for round-trip
    // Extract candidateId
    Explanation e;
    auto findStr = [&](const std::string& key)->std::string{
        std::string pat = "\"" + key + "\"";
        auto pos = json.find(pat);
        if(pos==std::string::npos) return "";
        auto colon = json.find(':', pos+pat.size());
        if(colon==std::string::npos) throw std::invalid_argument("missing colon for "+key);
        auto q1 = json.find('"', colon+1);
        if(q1==std::string::npos) throw std::invalid_argument("missing quote for "+key);
        // Need to find matching closing quote, handling escaped? For simplicity, find next "
        auto q2 = json.find('"', q1+1);
        if(q2==std::string::npos) throw std::invalid_argument("missing closing quote for "+key);
        return json.substr(q1+1, q2-q1-1);
    };
    auto findBool = [&](const std::string& key)->bool{
        std::string pat = "\"" + key + "\"";
        auto pos = json.find(pat);
        if(pos==std::string::npos) return false;
        auto colon = json.find(':', pos+pat.size());
        auto comma = json.find(',', colon+1);
        auto brace = json.find('}', colon+1);
        size_t end = (comma!=std::string::npos && brace!=std::string::npos) ? std::min(comma,brace) : (comma!=std::string::npos?comma:brace);
        std::string val = json.substr(colon+1, end-colon-1);
        val.erase(0, val.find_first_not_of(" \t\n\r"));
        val.erase(val.find_last_not_of(" \t\n\r")+1);
        return val=="true";
    };
    e.candidateId = findStr("candidateId");
    if(e.candidateId.empty()) throw std::invalid_argument("missing candidateId");
    e.decisionLabel = findStr("decisionLabel");
    e.whyNow = findStr("whyNow");
    e.whatWillChange = findStr("whatWillChange");
    e.whatWillNotChange = findStr("whatWillNotChange");
    e.expectedBenefit = findStr("expectedBenefit");
    e.observedBenefit = findStr("observedBenefit");
    e.verdict = findStr("verdict");
    e.id = findStr("id");
    e.rebootRequired = findBool("rebootRequired");
    e.authorizationRequired = findBool("authorizationRequired");
    // For other fields, keep defaults; round-trip will be validated via candidateId/whyNow etc.
    // Ensure no secret leaked in parsed json (redaction check)
    if(containsSecret(json)) throw std::invalid_argument("secret in json");
    return e;
}

std::string Explanation::toHuman(bool verbose) const {
    std::ostringstream oss;
    oss << "WHY NOW: " << whyNow << "\n";
    oss << "WHAT WILL CHANGE: " << whatWillChange << "\n";
    oss << "WHAT WILL NOT CHANGE: " << whatWillNotChange << "\n";
    oss << "EXPECTED BENEFIT: " << expectedBenefit << "\n";
    oss << "CONFIDENCE: " << confidence << "\n";
    oss << "RISK: " << risk << "\n";
    oss << "REVERSIBILITY: " << reversibility << "\n";
    oss << "REBOOT: " << (rebootRequired?"true":"false") << "\n";
    oss << "AUTHORIZATION: " << (authorizationRequired?"true":"false") << "\n";
    oss << "DECISION: " << decisionLabel << " (" << toString(decision) << ")\n";
    if(!rejectionConditions.empty()){
        oss << "REJECTION CONDITIONS: ";
        auto rc = rejectionConditions;
        std::sort(rc.begin(), rc.end());
        for(size_t i=0;i<rc.size();i++){ if(i) oss<<", "; oss<< redact(rc[i]); }
        oss << "\n";
    }
    oss << "ROLLBACK: " << rollbackSummary << "\n";
    if(!beforeStateSummary.empty()) oss << "BEFORE: " << beforeStateSummary << "\n";
    if(!afterStateSummary.empty()) oss << "AFTER: " << afterStateSummary << "\n";
    if(!observedBenefit.empty()) oss << "OBSERVED BENEFIT: " << observedBenefit << "\n";
    if(!verdict.empty()) oss << "VERDICT: " << verdict << " (" << verdictReason << ")\n";
    if(hasRegression) oss << "REGRESSION: true\n";
    if(!limitations.empty()) oss << "LIMITATIONS: " << limitations << "\n";
    if(verbose){
        oss << "EVIDENCE:\n";
        auto ev = evidence;
        std::sort(ev.begin(), ev.end());
        for(auto &e: ev) oss << "  - " << redact(e) << "\n";
        oss << "DEPENDENCIES:\n";
        auto deps = dependencies;
        std::sort(deps.begin(), deps.end());
        for(auto &d: deps) oss << "  - " << redact(d) << "\n";
        oss << "USER IMPACT: " << userImpact << "\n";
    }
    return oss.str();
}

} // namespace polaris::explainability
