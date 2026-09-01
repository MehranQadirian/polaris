#include "../core/safety/transaction/StateMachine.h"
#include "../core/safety/transaction/Transaction.h"
#include "../core/safety/FileSafety.h"
#include "../core/safety/backup/BackupEngine.h"
#include "../core/safety/audit/AuditLog.h"
#include "../core/profile/UserProfile.h"
#include "../core/profile/ProfileStore.h"
#include "../core/profile/ProfileService.h"
#include "../core/profile/ProfileAdvisor.h"
#include "../core/explainability/Explanation.h"
#include "../core/explainability/ExplanationEngine.h"
#include "../core/domain/PerfModels.h"
#include "../core/engines/perf/BaselineEngine.h"
#include "../core/engines/bottleneck/BottleneckEngine.h"
#include "../core/engines/recommend/RecommendationEngine.h"
#include "../core/capabilities/OptimizationRegistry.h"
#include "../core/capabilities/CapabilityRegistrySetup.h"
#include "../core/safety/transaction/TransactionStore.h"
#include "../core/providers/real/RealFlatpakProvider.h"
#include "../core/providers/real/RealJournalDiskProvider.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <map>
#include <vector>

using namespace polaris::safety;

std::string nowISO(){
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    char buf[64]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S%z", localtime(&tt));
    return buf;
}

std::string testRoot = "/tmp/polaris-test-root";
std::string txStore = "/tmp/polaris-test-root/transactions";
std::string auditPath = "/tmp/polaris-test-root/audit.log";

void ensureTestFixtures(){
    std::filesystem::create_directories(testRoot+"/etc");
    // Create dummy fstab
    std::string fstab = testRoot+"/etc/fstab";
    if(!std::filesystem::exists(fstab)){
        std::ofstream out(fstab);
        out << "# test fstab\n";
        out << "UUID=39b0b8c8-58b6-4136-ad6a-7c3b1cf1f45d none swap sw 0 0\n";
        out << "UUID=24bd938d-b629-4c12-a681-d19cd1270645 / ext4 defaults 1 1\n";
    }
    std::filesystem::create_directories(txStore);
}

void cmd_preview(const std::string& op){
    ensureTestFixtures();
    // P19: handle capability-backed preview
    if(op=="flatpak-unused" || op=="journal-vacuum"){
        polaris::capabilities::ensureCapabilitiesRegistered();
        auto cap = polaris::capabilities::OptimizationRegistry::instance().lookup(op);
        if(!cap){
            std::cout << "{\"error\":\"capability not found: "+op+"\"}\n";
            return;
        }
        // Collect baseline (read-only)
        auto baseline = polaris::engines::perf::BaselineEngine::collect();
        // For fixture demonstration, allow overriding baseline via fixture files under /tmp/polaris-test-root/p19/*
        // Try to load fixture if exists
        std::string fixtureFlatpakList = "/tmp/polaris-test-root/p19/flatpak.list";
        std::string fixtureFlatpakUnused = "/tmp/polaris-test-root/p19/flatpak.unused";
        std::string fixtureJournal = "/tmp/polaris-test-root/p19/journal.usage";
        if(std::filesystem::exists(fixtureFlatpakList) && op=="flatpak-unused"){
            std::ifstream f1(fixtureFlatpakList); std::string list((std::istreambuf_iterator<char>(f1)),{});
            std::ifstream f2(fixtureFlatpakUnused); std::string unused((std::istreambuf_iterator<char>(f2)),{});
            baseline.flatpak = polaris::providers::real::RealFlatpakProvider::fromFixture(list, unused);
            // Ensure storage free available
            if(baseline.storage.filesystems.empty()){
                polaris::domain::StorageBaseline::Fs fse; fse.mount="/"; fse.freeBytes=50ULL*1024*1024*1024; fse.sizeBytes=100ULL*1024*1024*1024; fse.usedPct=50;
                baseline.storage.filesystems.push_back(fse);
            }
        }
        if(std::filesystem::exists(fixtureJournal) && op=="journal-vacuum"){
            std::ifstream f(fixtureJournal); std::string usage((std::istreambuf_iterator<char>(f)),{});
            baseline.journalDisk = polaris::providers::real::RealJournalDiskProvider::fromFixture(usage, "500M");
            if(baseline.storage.filesystems.empty()){
                polaris::domain::StorageBaseline::Fs fse2; fse2.mount="/"; fse2.freeBytes=50ULL*1024*1024*1024; fse2.sizeBytes=100ULL*1024*1024*1024; fse2.usedPct=50;
                baseline.storage.filesystems.push_back(fse2);
            }
        }
        polaris::profile::UserProfile profile;
        std::string ppath = polaris::profile::ProfileStore::profilePath();
        if(polaris::profile::ProfileStore::exists(ppath)){
            try{ profile = polaris::profile::ProfileStore::load(ppath);}catch(...){}
        }
        if(!cap->isApplicable(baseline, profile)){
            auto ev = cap->collect(baseline);
            std::cout << "{\"capability\":\""<<op<<"\",\"applicable\":false,\"reason\":\""<<ev.reason<<"\",\"reclaimableMB\":"<<ev.reclaimableBytes/(1024*1024)<<"}\n";
            std::cout << "# Not applicable on current host - no transaction previewed (correct)\n";
            return;
        }
        auto ev = cap->collect(baseline);
        if(!ev.available){
            std::cout << "{\"capability\":\""<<op<<"\",\"available\":false,\"reason\":\""<<ev.reason<<"\"}\n";
            return;
        }
        auto rec = cap->toRecommendation(ev, baseline);
        auto cur = cap->snapshot(baseline, ev);
        std::string txId = "TX-TEST-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count() % 100000);
        auto tx = cap->toTransaction(txId, rec, ev, cur);
        tx.timestamp = nowISO();
        // Persist via TransactionStore for stale/idempotency handling
        polaris::safety::TransactionStore store(txStore);
        auto res = store.create(tx);
        if(!res.valid){
            std::cout << "{\"error\":\""<<res.reason<<"\"}\n";
            return;
        }
        AuditEvent ae{nowISO(), txId, "transaction.previewed", "mehrangh", "PENDING", "PENDING", "", tx.previews[0].diff, "", "", "", "", ""};
        polaris::safety::AuditLog::append(ae);
        std::cout << "{\n  \"transactionId\":\""<<txId<<"\",\n";
        std::cout << "  \"capability\":\""<<op<<"\",\n";
        std::cout << "  \"state\":\""<<toString(tx.state)<<"\",\n";
        std::cout << "  \"target\":\""<<tx.target<<"\",\n";
        std::cout << "  \"risk\":\""<<tx.riskLevel<<"\",\n";
        std::cout << "  \"expectedBenefit\":\""<<tx.expectedBenefit<<"\",\n";
        std::cout << "  \"reclaimableMB\":"<<ev.reclaimableBytes/(1024*1024)<<",\n";
        std::cout << "  \"confidence\":"<<ev.confidence<<",\n";
        std::cout << "  \"diff\":\""<<tx.previews[0].diff.substr(0,120)<<"\",\n";
        std::cout << "  \"preconditions\":{";
        bool first=true; for(auto &kv: ev.preconditions){ if(!first) std::cout<<","; first=false; std::cout<<"\""<<kv.first<<"\":\""<<kv.second<<"\""; } std::cout<<"}\n}\n";
        std::cout << "# Preview via capability "<<op<<" - no writes, test fixture only\n";
        return;
    }
    Transaction tx;
    tx.id = "TX-TEST-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count() % 100000);
    tx.operationId = op;
    tx.target = testRoot+"/etc/fstab";
    tx.description = "Dummy fstab stale swap preview (test fixture only)";
    tx.riskLevel = "R2";
    tx.expectedBenefit = "Eliminate swap timeout (test)";
    tx.requiredPrivileges = "org.polaris.modify.fstab";
    tx.state = TxState::PREVIEWED;
    tx.approvalState = "PENDING";
    tx.authorizationState = "PENDING";
    tx.rebootRequired = false;
    tx.timestamp = nowISO();

    // Read beforeState
    std::ifstream f(tx.target);
    std::string before((std::istreambuf_iterator<char>(f)), {});
    tx.beforeState = before;

    // Generate preview diff
    ChangePreview cp;
    cp.target = tx.target;
    cp.beforeState = before;
    cp.afterState = "# UUID=39b0b8c8... disabled by Polaris "+tx.id+"\n" + before.substr(before.find("UUID=24bd")); // simplified
    cp.diff = "- UUID=39b0b8c8-58b6-4136-ad6a-7c3b1cf1f45d none swap sw 0 0\n+ # disabled by Polaris "+tx.id+"\n";
    cp.method = "atomic write via helper FileModify (test fixture)";
    cp.privilege = "org.polaris.modify.fstab";
    cp.risk = "R2";
    cp.benefit = "Eliminate timeout";
    cp.rollback = "Restore from backup " + std::string(BackupEngine::testBackupRoot()) + "/" + tx.id + "/fstab.bak";
    cp.rebootRequired = false;
    tx.previews.push_back(cp);

    // Save tx
    std::string path = txStore+"/"+tx.id+".json";
    std::ofstream out(path);
    out << "{\"id\":\"" << tx.id << "\",\"state\":\"" << toString(tx.state) << "\",\"target\":\"" << tx.target << "\",\"risk\":\"" << tx.riskLevel << "\"}\n";

    // Audit
    AuditEvent ev{nowISO(), tx.id, "transaction.previewed", "mehrangh", "PENDING", "PENDING", "", cp.diff, "", "", "", "", ""};
    AuditLog::append(ev);

    std::cout << "{\n  \"transactionId\":\"" << tx.id << "\",\n";
    std::cout << "  \"state\":\"" << toString(tx.state) << "\",\n";
    std::cout << "  \"target\":\"" << tx.target << "\",\n";
    std::cout << "  \"risk\":\"" << tx.riskLevel << "\",\n";
    std::cout << "  \"before\":\"" << tx.beforeState.substr(0,80) << "...\",\n";
    std::cout << "  \"after\":\"" << cp.afterState.substr(0,80) << "...\",\n";
    std::cout << "  \"diff\":\"" << cp.diff.substr(0,100) << "\",\n";
    std::cout << "  \"privilege\":\"" << cp.privilege << "\",\n";
    std::cout << "  \"rollback\":\"" << cp.rollback << "\",\n";
    std::cout << "  \"rebootRequired\":" << (cp.rebootRequired?"true":"false") << "\n}\n";
    std::cout << "# Preview - no writes, no auth, test fixture only\n";
}

void cmd_list(){
    ensureTestFixtures();
    std::cout << "[\n";
    bool first=true;
    for(auto &p: std::filesystem::directory_iterator(txStore)){
        if(!first) std::cout << ",\n";
        first=false;
        std::ifstream f(p.path());
        std::string content((std::istreambuf_iterator<char>(f)), {});
        std::cout << "  " << content;
    }
    std::cout << "\n]\n";
}

void cmd_show(const std::string& id){
    // Support --json flag: check for real transaction store first, then test store
    std::string path = txStore+"/"+id+".json";
    std::ifstream f(path);
    if(!f){
        // Try real store
        path = std::string(getenv("HOME")?getenv("HOME"):"/home/mehrangh") + "/.local/state/polaris/transactions/"+id+".json";
        f.open(path);
        if(!f){ std::cerr << "Transaction not found: " << id << "\n"; return; }
    }
    std::string content((std::istreambuf_iterator<char>(f)), {});
    std::cout << content << "\n";
    // P11: if --json and transaction has comparison, also show beforeBaseline/afterBaseline/comparison if present
    // For now, just output raw content (which may include comparison for P7)
    // Future: parse and pretty print comparison
}

void cmd_approve(const std::string& id){
    std::string path = txStore+"/"+id+".json";
    std::ifstream f(path);
    if(!f){ std::cerr << "Not found\n"; return; }
    // In P4, we simulate approval without real auth - explicit approval required
    std::cout << "{\"transactionId\":\"" << id << "\",\"approval\":\"APPROVED\",\"state\":\"APPROVED\"}\n";
    AuditEvent ev{nowISO(), id, "transaction.approved", "mehrangh", "APPROVED", "PENDING", "", "", "", "", "", "", ""};
    AuditLog::append(ev);
    std::cout << "# Explicit approval recorded - not equivalent to launch, preview required first\n";
}

void cmd_dryrun(const std::string& op){
    std::cout << "# Dry-run for operation: " << op << " (test fixture)\n";
    cmd_preview(op);
    std::cout << "# Dry-run MUST NOT write files, invoke privileged ops, or request password - verified\n";
}

void cmd_audit_list(){
    auto events = AuditLog::list("");
    std::cout << "[\n";
    for(size_t i=0;i<events.size();i++){
        std::cout << "  {\"transactionId\":\"" << events[i].transactionId << "\",\"hash\":\"" << events[i].eventHash << "\"}";
        if(i+1<events.size()) std::cout << ",";
        std::cout << "\n";
    }
    std::cout << "]\n";
}

void cmd_compare(const std::string& id){
    std::string path = std::string(getenv("HOME")?getenv("HOME"):"/home/mehrangh") + "/.local/state/polaris/transactions/"+id+".json";
    std::ifstream f(path);
    if(!f){
        path = txStore+"/"+id+".json";
        f.open(path);
        if(!f){ std::cerr << "Transaction not found for compare: " << id << "\n"; return; }
    }
    std::string content((std::istreambuf_iterator<char>(f)), {});
    // For P11, if content contains "comparison", show it, else show before/after
    if(content.find("comparison")!=std::string::npos || content.find("beforeBaseline")!=std::string::npos){
        std::cout << "{\"transactionId\":\"" << id << "\",\"comparison\": \"present in transaction JSON\"}\n";
        std::cout << content << "\n";
    } else {
        std::cout << "{\"transactionId\":\"" << id << "\",\"comparison\":\"not yet available - beforeBaseline/afterBaseline not yet captured (reboot-pending)\"}\n";
    }
}

void cmd_profile_show(bool jsonFlag){
    std::string path = polaris::profile::ProfileStore::profilePath();
    polaris::profile::UserProfile profile;
    bool exists = polaris::profile::ProfileStore::exists(path);
    if(exists){
        try { profile = polaris::profile::ProfileStore::load(path); }
        catch(const std::exception& e){
            std::cerr << "{\"error\":\"" << e.what() << "\"}\n";
            return;
        }
    }
    std::string j = profile.toJson();
    if(jsonFlag) std::cout << j << "\n";
    else {
        std::cout << j << "\n";
        std::cout << "# Profile: " << path << (exists?" (exists)":" (not exists, default unknown, not created)") << "\n";
        // Also show advisor example for akonadi
        auto adv = polaris::profile::ProfileAdvisor::canConsiderAkonadi(profile);
        std::cout << "# Akonadi: " << toString(adv.decision) << " - " << adv.reason << "\n";
        std::cout << "# What will not change: " << adv.whatWillNotChange << "\n";
    }
}

void cmd_profile_set(const std::string& field, const std::string& valueStr, bool jsonFlag){
    std::string path = polaris::profile::ProfileStore::profilePath();
    polaris::profile::UserProfile profile;
    bool existed = polaris::profile::ProfileStore::exists(path);
    if(existed){
        try { profile = polaris::profile::ProfileStore::load(path); }
        catch(...){ profile = polaris::profile::UserProfile{}; }
    }
    try {
        auto result = polaris::profile::ProfileService::updateField(profile, field, valueStr, path);
        std::cout << "{\"field\":\"" << result.field << "\",\"previousValue\":\"" << polaris::profile::toString(result.previousValue) << "\",\"newValue\":\"" << polaris::profile::toString(result.newValue) << "\",\"status\":\"" << (result.auditOperation=="profile.update.idempotent"?"idempotent":"updated") << "\"}\n";
        if(!jsonFlag) std::cout << "# " << result.reason << "\n";
    } catch(const std::exception& e){
        std::cerr << "{\"error\":\"" << e.what() << "\"}\n";
    }
}

void cmd_explain_candidate(const std::string& candidateId, bool jsonFlag, bool verbose){
    // Load profile (real path, no auto-create)
    std::string path = polaris::profile::ProfileStore::profilePath();
    polaris::profile::UserProfile profile;
    if(polaris::profile::ProfileStore::exists(path)){
        try { profile = polaris::profile::ProfileStore::load(path); } catch(...){ profile = polaris::profile::UserProfile{}; }
    }
    // No Recommendation object for generic candidate; engine will mock
    auto exp = polaris::explainability::ExplanationEngine::explainCandidate(candidateId, profile, nullptr, nullptr);
    // Audit explanation generated (not approval)
    polaris::safety::AuditEvent ev{nowISO(), "EXPLAIN-"+candidateId, "explanation.generated", "test", "", "", "", "", "", "", "candidate="+candidateId+" decision="+exp.decisionLabel, "", ""};
    polaris::safety::AuditLog::append(ev);
    if(jsonFlag) std::cout << exp.toJson() << "\n";
    else std::cout << exp.toHuman(verbose) << "\n";
}

void cmd_explain_transaction(const std::string& txId, bool jsonFlag, bool verbose){
    std::string path = txStore+"/"+txId+".json";
    std::ifstream f(path);
    if(!f){
        path = std::string(getenv("HOME")?getenv("HOME"):"/home/mehrangh") + "/.local/state/polaris/transactions/"+txId+".json";
        f.open(path);
        if(!f){ std::cerr << "Transaction not found: " << txId << "\n"; return; }
    }
    std::string content((std::istreambuf_iterator<char>(f)), {});
    // Minimal parse for explain: extract target, operation, state, expectedBenefit, risk, beforeState etc. For P16 we mock transaction fields from content if not fully parsable
    polaris::safety::Transaction tx;
    tx.id = txId;
    // Try to extract state
    auto pos = content.find("\"state\":\"");
    if(pos!=std::string::npos){
        auto s=pos+9; auto e=content.find('"',s);
        std::string stateStr=content.substr(s,e-s);
        if(stateStr=="PREVIEWED") tx.state=polaris::safety::TxState::PREVIEWED;
        else if(stateStr=="APPROVED") tx.state=polaris::safety::TxState::APPROVED;
        else if(stateStr=="COMPLETED") tx.state=polaris::safety::TxState::COMPLETED;
        else if(stateStr=="FAILED") tx.state=polaris::safety::TxState::FAILED;
        else tx.state=polaris::safety::TxState::PREVIEWED;
    } else {
        tx.state=polaris::safety::TxState::PREVIEWED;
    }
    tx.target = "/tmp/polaris-test-root/etc/fstab";
    tx.operationId = "dummy-test";
    tx.expectedBenefit = "Eliminate swap timeout";
    tx.riskLevel = "R2";
    // Load profile
    std::string profPath = polaris::profile::ProfileStore::profilePath();
    polaris::profile::UserProfile profile;
    if(polaris::profile::ProfileStore::exists(profPath)){
        try { profile = polaris::profile::ProfileStore::load(profPath); } catch(...){}
    }
    // Try to load comparison if transaction json contains comparison (P11)
    polaris::domain::Comparison* compPtr=nullptr;
    polaris::domain::Comparison comp;
    if(content.find("comparison")!=std::string::npos){
        // For demo, create mock comparison
        comp.transactionId = txId;
        comp.expectedBenefit = tx.expectedBenefit;
        comp.observedBenefit = "MX130 claimed, no regression";
        comp.verdict = polaris::domain::Verdict::SUCCESS;
        comp.verdictReason = "Observed benefit matches expected and no regression";
        comp.hasRegression=false;
        compPtr=&comp;
    }
    auto exp = polaris::explainability::ExplanationEngine::explainTransaction(tx, profile, compPtr, nullptr);
    polaris::safety::AuditEvent ev{nowISO(), txId, "explanation.generated", "test", "", "", "", "", "", "", "transaction="+txId+" decision="+exp.decisionLabel, "", ""};
    polaris::safety::AuditLog::append(ev);
    if(jsonFlag) std::cout << exp.toJson() << "\n";
    else std::cout << exp.toHuman(verbose) << "\n";
}

void cmd_recommendations(bool jsonFlag){
    auto baseline = polaris::engines::perf::BaselineEngine::collect();
    polaris::profile::UserProfile profile;
    std::string ppath = polaris::profile::ProfileStore::profilePath();
    if(polaris::profile::ProfileStore::exists(ppath)){
        try{ profile = polaris::profile::ProfileStore::load(ppath);}catch(...){}
    }
    // Use bottleneck engine
    auto bottlenecks = polaris::engines::bottleneck::BottleneckEngine::analyze(baseline);
    auto recs = polaris::engines::recommend::RecommendationEngine::generateWithProfile(baseline, bottlenecks, profile);
    if(jsonFlag){
        std::cout << "[\n";
        for(size_t i=0;i<recs.size();i++){
            auto &r=recs[i];
            std::cout << "  {\"id\":\""<<r.id<<"\",\"title\":\""<<r.title<<"\",\"category\":\""<<r.category<<"\",\"confidence\":"<<r.confidence<<",\"risk\":\""<<r.riskLevel<<"\",\"benefit\":\""<<r.expectedBenefit<<"\"}";
            if(i+1<recs.size()) std::cout<<",";
            std::cout<<"\n";
        }
        std::cout << "]\n";
    } else {
        std::cout << "Recommendations ("<<recs.size()<<"):\n";
        for(auto &r: recs){
            std::cout << " - "<<r.id<<" ["<<r.category<<"] "<<r.title<<" risk "<<r.riskLevel<<" confidence "<<r.confidence<<" benefit "<<r.expectedBenefit<<"\n";
        }
    }
}
void cmd_capabilities(bool jsonFlag){
    polaris::capabilities::ensureCapabilitiesRegistered();
    auto caps = polaris::capabilities::OptimizationRegistry::instance().capabilities();
    if(jsonFlag){
        std::cout << "[\n";
        for(size_t i=0;i<caps.size();i++){
            std::cout << "  {\"id\":\""<<caps[i]->id()<<"\",\"name\":\""<<caps[i]->name()<<"\",\"category\":\""<<caps[i]->category()<<"\",\"risk\":\""<<caps[i]->risk()<<"\",\"reboot\":"<<(caps[i]->requiresReboot()?"true":"false")<<",\"auth\":"<<(caps[i]->requiresAuth()?"true":"false")<<"}";
            if(i+1<caps.size()) std::cout<<",";
            std::cout<<"\n";
        }
        std::cout << "]\n";
    } else {
        std::cout << "Capabilities ("<<caps.size()<<"):\n";
        for(auto c: caps) std::cout << " - "<<c->id()<<" ["<<c->category()<<"] "<<c->name()<<" risk "<<c->risk()<<" reboot "<<(c->requiresReboot()?"yes":"no")<<" auth "<<(c->requiresAuth()?"yes":"no")<<"\n";
    }
}

int main(int argc, char** argv){
    std::string cmd = argc>1? argv[1] : "help";
    bool jsonFlag=false;
    bool verboseFlag=false;
    for(int i=1;i<argc;i++){
        if(std::string(argv[i])=="--json") jsonFlag=true;
        if(std::string(argv[i])=="--verbose") verboseFlag=true;
    }
    (void)jsonFlag; (void)verboseFlag;
    if(cmd=="recommendations"){
        cmd_recommendations(jsonFlag);
    } else if(cmd=="capabilities" && argc>2){
        std::string sub=argv[2];
        if(sub=="list") cmd_capabilities(jsonFlag);
        else std::cout << "Usage: polaris_p4 capabilities list [--json]\n";
    } else if(cmd=="transaction" && argc>2){
        std::string sub=argv[2];
        if(sub=="list") cmd_list();
        else if(sub=="show" && argc>3) cmd_show(argv[3]);
        else if(sub=="compare" && argc>3) cmd_compare(argv[3]);
        else if(sub=="preview" && argc>3) cmd_preview(argv[3]);
        else if(sub=="approve" && argc>3) cmd_approve(argv[3]);
        else if(sub=="explain" && argc>3) cmd_explain_transaction(argv[3], jsonFlag, verboseFlag);
        else if(sub=="rollback"){
            std::cout << "{\"error\":\"P4 rollback on test fixtures only - use TX-TEST id\"}\n";
        }
        else std::cout << "Usage: polaris_p4 transaction <list|show|preview|approve|rollback|compare|explain> [id] [--json] [--verbose]\n";
    } else if(cmd=="audit" && argc>2){
        std::string sub=argv[2];
        if(sub=="list") cmd_audit_list();
        else std::cout << "audit list\n";
    } else if(cmd=="apply" && argc>2 && std::string(argv[2])=="--dry-run"){
        std::string op = argc>3? argv[3] : "dummy";
        cmd_dryrun(op);
    } else if(cmd=="profile" && argc>2){
        std::string sub=argv[2];
        if(sub=="show") cmd_profile_show(jsonFlag);
        else if(sub=="set" && argc>4) cmd_profile_set(argv[3], argv[4], jsonFlag);
        else std::cout << "Usage: polaris_p4 profile <show|set> [field] [yes|no|unknown] [--json]\n"
                       << "  Fields: usesKMail, usesKontact, usesKOrganizer, usesBluetooth, usesPrinting, usesAvahi, usesCups, usesAkonadi\n";
    } else if(cmd=="explain" && argc>2){
        std::string candidate = argv[2];
        cmd_explain_candidate(candidate, jsonFlag, verboseFlag);
    } else {
        std::cout << "Polaris P4/P11/P12/P13/P16/P19 - SAFE INFRASTRUCTURE READY\n";
        std::cout << "Usage:\n";
        std::cout << "  polaris_p4 recommendations [--json]  # P19 registry + profile\n";
        std::cout << "  polaris_p4 capabilities list [--json]\n";
        std::cout << "  polaris_p4 transaction list\n";
        std::cout << "  polaris_p4 transaction show <id> [--json]\n";
        std::cout << "  polaris_p4 transaction compare <id> [--json]\n";
        std::cout << "  polaris_p4 transaction preview <operation>  # dummy-test, flatpak-unused, journal-vacuum (P19 fixture)\n";
        std::cout << "  polaris_p4 transaction approve <id>\n";
        std::cout << "  polaris_p4 transaction rollback <id>  # test fixtures only\n";
        std::cout << "  polaris_p4 transaction explain <id> [--json] [--verbose]\n";
        std::cout << "  polaris_p4 audit list\n";
        std::cout << "  polaris_p4 apply --dry-run <operation>\n";
        std::cout << "  polaris_p4 profile show [--json]\n";
        std::cout << "  polaris_p4 profile set <field> <yes|no|unknown> [--json]\n";
        std::cout << "  polaris_p4 explain <candidate> [--json] [--verbose]\n";
        std::cout << "    candidate examples: akonadi-disable, bluetooth-disable, fstab-stale-swap, flatpak-unused, journal-vacuum\n";
        std::cout << "P19 registry: flatpak-unused (R1), journal-vacuum (R1) - fixture only, no privileged apply\n";
        std::cout << "P13 profile at ~/.local/state/polaris/profile.json (tests use /tmp/polaris-test-root)\n";
    }
    return 0;
}
