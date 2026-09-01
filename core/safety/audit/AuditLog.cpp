#include "AuditLog.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>

namespace polaris::safety {

std::string AuditLog::hashEvent(const AuditEvent& e){
    std::string data = e.timestamp + e.transactionId + e.operation + e.user + e.approval + e.authorizationResult + e.previousHash;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.size(), hash);
    std::ostringstream oss;
    for(int i=0;i<SHA256_DIGEST_LENGTH;i++) oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

void AuditLog::append(const AuditEvent& e){
    // Use test log if transactionId starts with TX-TEST
    std::string path = logPath();
    if(e.transactionId.rfind("TX-TEST",0)==0) path = testLogPath();
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    AuditEvent ev = e;
    // Get previous hash
    std::string prev;
    {
        std::ifstream f(path);
        std::string line, last;
        while(std::getline(f,line)) last=line;
        if(!last.empty()){
            // last line is JSON, find eventHash
            auto pos = last.find("\"eventHash\":\"");
            if(pos!=std::string::npos){
                auto start=pos+13;
                auto end=last.find('"',start);
                prev=last.substr(start,end-start);
            }
        }
    }
    ev.previousHash = prev;
    ev.eventHash = hashEvent(ev);
    std::ofstream out(path, std::ios::app);
    out << "{\"timestamp\":\"" << ev.timestamp << "\","
        << "\"transactionId\":\"" << ev.transactionId << "\","
        << "\"operation\":\"" << ev.operation << "\","
        << "\"user\":\"" << ev.user << "\","
        << "\"previousHash\":\"" << ev.previousHash << "\","
        << "\"eventHash\":\"" << ev.eventHash << "\""
        << ",\"error\":\"" << ev.error << "\"}\n";
    out.flush();
    // P12 hardening: fsync per event for crash safety (hash chain integrity)
    if(out){
        int fd = ::open(path.c_str(), O_RDONLY);
        if(fd >= 0){
            ::fsync(fd);
            ::close(fd);
        }
    }
}

std::vector<AuditEvent> AuditLog::list(const std::string& transactionId){
    std::vector<AuditEvent> out;
    for(auto path : {logPath(), testLogPath()}){
        std::ifstream f(path);
        if(!f) continue;
        std::string line;
        while(std::getline(f,line)){
            if(!transactionId.empty() && line.find(transactionId)==std::string::npos) continue;
            AuditEvent e;
            // minimal parse: extract transactionId
            auto pos=line.find("\"transactionId\":\"");
            if(pos!=std::string::npos){
                auto s=pos+17; auto epos=line.find('"',s);
                e.transactionId=line.substr(s,epos-s);
            }
            e.error=line; // raw
            out.push_back(e);
        }
    }
    return out;
}

std::optional<AuditEvent> AuditLog::get(const std::string& eventHash){
    for(auto path : {logPath(), testLogPath()}){
        std::ifstream f(path);
        std::string line;
        while(std::getline(f,line)){
            if(line.find(eventHash)!=std::string::npos){
                AuditEvent e; e.eventHash=eventHash; e.error=line; return e;
            }
        }
    }
    return std::nullopt;
}

} // namespace polaris::safety
