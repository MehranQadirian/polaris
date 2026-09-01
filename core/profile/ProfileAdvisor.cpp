#include "ProfileAdvisor.h"

namespace polaris::profile {

AdvisorResult ProfileAdvisor::canConsiderAkonadi(const UserProfile& p){
    AdvisorResult r;
    r.candidate = "akonadi";
    // Check YES first
    if(p.usesKMail==TriState::YES){
        r.decision = Decision::BLOCKED_BY_USER_WORKFLOW;
        r.causingField = "usesKMail";
        r.causingValue = "yes";
        r.explicitFact = true;
        r.reason = "Akonadi optimization is blocked because usesKMail=yes. Akonadi must remain available for the user's KMail workflow.";
        r.whatWillNotChange = "Akonadi will remain enabled and running; 14 agents, 1302M, db 126M will not be disabled.";
        r.confirmationRequired = "User must explicitly set usesKMail=no, usesKontact=no, usesKOrganizer=no and usesAkonadi=no to consider (current: usesKMail=yes).";
        return r;
    }
    if(p.usesKontact==TriState::YES){
        r.decision = Decision::BLOCKED_BY_USER_WORKFLOW;
        r.causingField = "usesKontact";
        r.causingValue = "yes";
        r.explicitFact = true;
        r.reason = "Akonadi optimization is blocked because usesKontact=yes. Akonadi must remain available for the user's Kontact workflow.";
        r.whatWillNotChange = "Akonadi will remain enabled and running; 14 agents, 1302M, db 126M will not be disabled.";
        r.confirmationRequired = "User must explicitly set usesKMail=no, usesKontact=no, usesKOrganizer=no and usesAkonadi=no to consider (current: usesKontact=yes).";
        return r;
    }
    if(p.usesKOrganizer==TriState::YES){
        r.decision = Decision::BLOCKED_BY_USER_WORKFLOW;
        r.causingField = "usesKOrganizer";
        r.causingValue = "yes";
        r.explicitFact = true;
        r.reason = "Akonadi optimization is blocked because usesKOrganizer=yes. Akonadi must remain available for the user's KOrganizer workflow.";
        r.whatWillNotChange = "Akonadi will remain enabled and running; 14 agents, 1302M, db 126M will not be disabled.";
        r.confirmationRequired = "User must explicitly set usesKMail=no, usesKontact=no, usesKOrganizer=no and usesAkonadi=no to consider (current: usesKOrganizer=yes).";
        return r;
    }
    if(p.usesAkonadi==TriState::YES){
        r.decision = Decision::BLOCKED_BY_USER_WORKFLOW;
        r.causingField = "usesAkonadi";
        r.causingValue = "yes";
        r.explicitFact = true;
        r.reason = "Akonadi optimization is blocked because usesAkonadi=yes. Akonadi must remain available for the user's Akonadi workflow.";
        r.whatWillNotChange = "Akonadi will remain enabled and running; 14 agents, 1302M, db 126M will not be disabled.";
        r.confirmationRequired = "User must explicitly set usesKMail=no, usesKontact=no, usesKOrganizer=no and usesAkonadi=no to consider (current: usesAkonadi=yes).";
        return r;
    }
    // Check UNKNOWN
    if(p.usesKMail==TriState::UNKNOWN || p.usesKontact==TriState::UNKNOWN || p.usesKOrganizer==TriState::UNKNOWN || p.usesAkonadi==TriState::UNKNOWN){
        r.decision = Decision::REQUIRES_USER_CONFIRMATION;
        // Find first unknown
        if(p.usesKMail==TriState::UNKNOWN) { r.causingField="usesKMail"; r.causingValue="unknown"; }
        else if(p.usesKontact==TriState::UNKNOWN) { r.causingField="usesKontact"; r.causingValue="unknown"; }
        else if(p.usesKOrganizer==TriState::UNKNOWN) { r.causingField="usesKOrganizer"; r.causingValue="unknown"; }
        else { r.causingField="usesAkonadi"; r.causingValue="unknown"; }
        r.explicitFact = false;
        r.reason = "Akonadi optimization requires user confirmation because "+r.causingField+"=unknown. User workflow not yet declared.";
        r.whatWillNotChange = "Akonadi will remain enabled until user explicitly confirms workflow (no host mutation will occur).";
        r.confirmationRequired = "User must explicitly set usesKMail, usesKontact, usesKOrganizer and usesAkonadi to yes/no (currently "+r.causingField+"=unknown).";
        return r;
    }
    // All NO
    r.decision = Decision::ALLOWED_FOR_ANALYSIS;
    r.causingField = "usesKMail";
    r.causingValue = "no";
    r.explicitFact = true;
    r.reason = "Akonadi optimization may be considered for analysis because user explicitly declared usesKMail=no, usesKontact=no, usesKOrganizer=no, usesAkonadi=no. This does NOT authorize mutation; still requires RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY.";
    r.whatWillNotChange = "Akonadi will remain enabled until a future transaction is previewed, explicitly approved, backed up, and applied - profile alone does not mutate the host.";
    r.confirmationRequired = "None: explicit no allows analysis, but any future real-host mutation still requires explicit transaction approval.";
    return r;
}

AdvisorResult ProfileAdvisor::canConsiderBluetooth(const UserProfile& p){
    AdvisorResult r; r.candidate="bluetooth";
    if(p.usesBluetooth==TriState::YES){
        r.decision = Decision::BLOCKED_BY_USER_WORKFLOW;
        r.causingField="usesBluetooth"; r.causingValue="yes"; r.explicitFact=true;
        r.reason = "Bluetooth service optimization is not recommended because the user explicitly declared usesBluetooth=yes.";
        r.whatWillNotChange = "Bluetooth service will remain enabled and active; paired devices TSCO-TS2343 will remain functional.";
        r.confirmationRequired = "User must explicitly set usesBluetooth=no to consider (current: usesBluetooth=yes).";
        return r;
    }
    if(p.usesBluetooth==TriState::UNKNOWN){
        r.decision = Decision::REQUIRES_USER_CONFIRMATION;
        r.causingField="usesBluetooth"; r.causingValue="unknown"; r.explicitFact=false;
        r.reason = "Bluetooth optimization requires user confirmation because usesBluetooth=unknown. User workflow not yet declared.";
        r.whatWillNotChange = "Bluetooth will remain unchanged until user explicitly declares usesBluetooth.";
        r.confirmationRequired = "User must explicitly set usesBluetooth to yes/no (currently unknown).";
        return r;
    }
    r.decision = Decision::ALLOWED_FOR_ANALYSIS;
    r.causingField="usesBluetooth"; r.causingValue="no"; r.explicitFact=true;
    r.reason = "Bluetooth optimization may be considered for analysis because user explicitly declared usesBluetooth=no. This does NOT authorize mutation; still requires RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY.";
    r.whatWillNotChange = "Bluetooth will remain enabled until a future transaction is previewed, approved, backed up, and applied.";
    r.confirmationRequired = "None: explicit no allows analysis, but any future mutation still requires explicit approval.";
    return r;
}

AdvisorResult ProfileAdvisor::canConsiderPrinting(const UserProfile& p){
    AdvisorResult r; r.candidate="printing";
    // Printing blocked if usesPrinting or usesCups is yes
    if(p.usesPrinting==TriState::YES){
        r.decision = Decision::BLOCKED_BY_USER_WORKFLOW;
        r.causingField="usesPrinting"; r.causingValue="yes"; r.explicitFact=true;
        r.reason = "Printing optimization is not recommended because the user explicitly declared usesPrinting=yes.";
        r.whatWillNotChange = "CUPS and printing will remain available; socket-activated cups will not be disabled.";
        r.confirmationRequired = "User must explicitly set usesPrinting=no and usesCups=no to consider (current: usesPrinting=yes).";
        return r;
    }
    if(p.usesCups==TriState::YES){
        r.decision = Decision::BLOCKED_BY_USER_WORKFLOW;
        r.causingField="usesCups"; r.causingValue="yes"; r.explicitFact=true;
        r.reason = "Printing optimization is not recommended because the user explicitly declared usesCups=yes.";
        r.whatWillNotChange = "CUPS and printing will remain available; socket-activated cups will not be disabled.";
        r.confirmationRequired = "User must explicitly set usesPrinting=no and usesCups=no to consider (current: usesCups=yes).";
        return r;
    }
    if(p.usesPrinting==TriState::UNKNOWN || p.usesCups==TriState::UNKNOWN){
        r.decision = Decision::REQUIRES_USER_CONFIRMATION;
        r.causingField = (p.usesPrinting==TriState::UNKNOWN)?"usesPrinting":"usesCups";
        r.causingValue="unknown"; r.explicitFact=false;
        r.reason = "Printing optimization requires user confirmation because "+r.causingField+"=unknown. User workflow not yet declared.";
        r.whatWillNotChange = "Printing will remain unchanged until user explicitly declares workflow.";
        r.confirmationRequired = "User must explicitly set usesPrinting and usesCups to yes/no (currently "+r.causingField+"=unknown).";
        return r;
    }
    r.decision = Decision::ALLOWED_FOR_ANALYSIS;
    r.causingField="usesPrinting"; r.causingValue="no"; r.explicitFact=true;
    r.reason = "Printing optimization may be considered for analysis because user explicitly declared usesPrinting=no and usesCups=no. This does NOT authorize mutation; still requires RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY.";
    r.whatWillNotChange = "Printing will remain enabled until a future transaction is previewed, approved, backed up, and applied.";
    r.confirmationRequired = "None: explicit no allows analysis, but any future mutation still requires explicit approval.";
    return r;
}

AdvisorResult ProfileAdvisor::canConsiderAvahi(const UserProfile& p){
    AdvisorResult r; r.candidate="avahi";
    if(p.usesAvahi==TriState::YES){
        r.decision = Decision::BLOCKED_BY_USER_WORKFLOW;
        r.causingField="usesAvahi"; r.causingValue="yes"; r.explicitFact=true;
        r.reason = "Avahi optimization is not recommended because the user explicitly declared usesAvahi=yes.";
        r.whatWillNotChange = "Avahi will remain enabled and active for kdeconnect.";
        r.confirmationRequired = "User must explicitly set usesAvahi=no to consider (current: usesAvahi=yes).";
        return r;
    }
    if(p.usesAvahi==TriState::UNKNOWN){
        r.decision = Decision::REQUIRES_USER_CONFIRMATION;
        r.causingField="usesAvahi"; r.causingValue="unknown"; r.explicitFact=false;
        r.reason = "Avahi optimization requires user confirmation because usesAvahi=unknown. User workflow not yet declared.";
        r.whatWillNotChange = "Avahi will remain unchanged until user explicitly declares usesAvahi.";
        r.confirmationRequired = "User must explicitly set usesAvahi to yes/no (currently unknown).";
        return r;
    }
    r.decision = Decision::ALLOWED_FOR_ANALYSIS;
    r.causingField="usesAvahi"; r.causingValue="no"; r.explicitFact=true;
    r.reason = "Avahi optimization may be considered for analysis because user explicitly declared usesAvahi=no. This does NOT authorize mutation; still requires RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY.";
    r.whatWillNotChange = "Avahi will remain enabled until a future transaction is previewed, approved, backed up, and applied.";
    r.confirmationRequired = "None: explicit no allows analysis, but any future mutation still requires explicit approval.";
    return r;
}

AdvisorResult ProfileAdvisor::canConsiderCups(const UserProfile& p){
    // Similar to printing but standalone cups
    return canConsiderPrinting(p);
}

AdvisorResult ProfileAdvisor::canConsider(const UserProfile& p, const std::string& candidateId){
    if(candidateId=="akonadi" || candidateId=="akonadi-disable" || candidateId=="CAND-AKONADI") return canConsiderAkonadi(p);
    if(candidateId=="bluetooth" || candidateId=="bluetooth-disable") return canConsiderBluetooth(p);
    if(candidateId=="printing" || candidateId=="cups" || candidateId=="cups-disable") return canConsiderPrinting(p);
    if(candidateId=="avahi" || candidateId=="avahi-disable") return canConsiderAvahi(p);
    // Generic fallback: unknown candidate requires confirmation if any relevant field unknown, else allowed
    AdvisorResult r; r.candidate=candidateId; r.decision=Decision::REQUIRES_USER_CONFIRMATION;
    r.reason="Candidate "+candidateId+" requires user confirmation because no explicit workflow mapping exists.";
    r.whatWillNotChange="Candidate will not be changed without explicit preview/approval.";
    r.confirmationRequired="User must declare relevant workflow.";
    r.explicitFact=false;
    return r;
}

} // namespace polaris::profile
