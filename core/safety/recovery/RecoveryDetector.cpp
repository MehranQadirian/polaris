#include "RecoveryDetector.h"
#include "../audit/AuditLog.h"
#include "../backup/BackupEngine.h"
#include <fstream>
#include <chrono>
#include <ctime>

namespace polaris::safety {

std::string RecoveryDetector::nowISO(){
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    char buf[64]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S%z", localtime(&tt));
    return buf;
}
void RecoveryDetector::audit(const std::string& op, const std::string& detail, const std::string& txId){
    AuditEvent ev;
    ev.timestamp = nowISO();
    ev.transactionId = txId.empty() ? "RECOVERY" : txId;
    ev.operation = op;
    ev.user = "test";
    ev.error = detail;
    AuditLog::append(ev);
}

bool RecoveryDetector::isIncomplete(TxState s){
    // Incomplete states that would be left after crash during BACKUP_CREATED/APPLYING/APPLIED/VERIFYING
    return s==TxState::BACKUP_CREATED || s==TxState::APPLYING || s==TxState::APPLIED || s==TxState::VERIFYING || s==TxState::AUTHORIZED;
}

TxState RecoveryDetector::parseState(const std::string& stateStr){
    if(stateStr=="BACKUP_CREATED") return TxState::BACKUP_CREATED;
    if(stateStr=="APPLYING") return TxState::APPLYING;
    if(stateStr=="APPLIED") return TxState::APPLIED;
    if(stateStr=="VERIFYING") return TxState::VERIFYING;
    if(stateStr=="VERIFIED") return TxState::VERIFIED;
    if(stateStr=="COMPLETED") return TxState::COMPLETED;
    if(stateStr=="FAILED") return TxState::FAILED;
    if(stateStr=="AUTHORIZED") return TxState::AUTHORIZED;
    if(stateStr=="AUTHORIZATION_REQUIRED") return TxState::AUTHORIZATION_REQUIRED;
    if(stateStr=="APPROVED") return TxState::APPROVED;
    if(stateStr=="PREVIEWED") return TxState::PREVIEWED;
    if(stateStr=="PROPOSED") return TxState::PROPOSED;
    if(stateStr=="CANCELLED") return TxState::CANCELLED;
    if(stateStr=="ROLLED_BACK") return TxState::ROLLED_BACK;
    if(stateStr=="ROLLING_BACK") return TxState::ROLLING_BACK;
    if(stateStr=="APPROVAL_REQUIRED") return TxState::APPROVAL_REQUIRED;
    return TxState::PROPOSED;
}

std::vector<RecoveryInfo> RecoveryDetector::detect(const std::string& storePath){
    std::vector<RecoveryInfo> out;
    std::error_code ec;
    if(!std::filesystem::exists(storePath, ec)) return out;
    for(auto &entry : std::filesystem::directory_iterator(storePath, ec)){
        if(ec) break;
        if(!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        if(path.find(".json")==std::string::npos) continue;
        std::ifstream f(path);
        if(!f) continue;
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // Extract id and state via simple search
        std::string id;
        auto idPos = content.find("\"id\":\"");
        if(idPos!=std::string::npos){
            auto s=idPos+6;
            auto e=content.find('"', s);
            if(e!=std::string::npos) id=content.substr(s, e-s);
        } else {
            // fallback filename
            id = entry.path().stem().string();
        }
        std::string stateStr;
        auto statePos = content.find("\"state\":\"");
        if(statePos!=std::string::npos){
            auto s=statePos+9;
            auto e=content.find('"', s);
            if(e!=std::string::npos) stateStr=content.substr(s, e-s);
        }
        TxState state = parseState(stateStr);
        if(isIncomplete(state)){
            RecoveryInfo info;
            info.id = id;
            info.state = state;
            // Check backup exists
            std::string backupDir = BackupEngine::testBackupRoot() + "/" + id;
            // Also check real backup root
            if(!std::filesystem::exists(backupDir)){
                backupDir = BackupEngine::backupRoot() + "/" + id;
            }
            info.backupExists = std::filesystem::exists(backupDir);
            if(info.backupExists){
                // Find any .bak file
                for(auto &bentry : std::filesystem::directory_iterator(backupDir, ec)){
                    if(bentry.is_regular_file()){
                        info.backupPath = bentry.path().string();
                        break;
                    }
                }
            }
            info.suggested = TxState::FAILED;
            info.reason = "incomplete transaction detected in state "+toString(state)+" - requires validation and approval, will not automatically mutate (fail-closed)";
            out.push_back(info);
            audit("recovery.detected", info.reason, id);
        }
    }
    return out;
}

} // namespace polaris::safety
