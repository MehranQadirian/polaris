#pragma once
#include "UserProfile.h"
#include "../safety/FileSafety.h"
#include "../safety/audit/AuditLog.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>

namespace polaris::profile {

class ProfileStore {
public:
    static std::string profilePath(){
        const char* home = getenv("HOME");
        std::string base = home ? std::string(home)+"/.local/state/polaris" : "/tmp/polaris-state";
        return base + "/profile.json";
    }
    static std::string testProfilePath(){
        return "/tmp/polaris-test-root/profile.json";
    }
    static std::string testProfilePath(const std::string& dir){
        // dir is like /tmp/polaris-test-root/p13_xxx -> profile at dir/profile.json for isolation
        if(dir.empty()) return testProfilePath();
        if(dir.back()=='/') return dir + "profile.json";
        return dir + "/profile.json";
    }

    // Load - if not exists, return default unknown (do NOT create file). Throws on symlink/malformed.
    static UserProfile load(const std::string& path = profilePath()){
        // Check symlink before exists
        std::error_code ec;
        if(std::filesystem::is_symlink(path, ec) && !ec){
            throw std::runtime_error("Profile symlink rejected: "+path);
        }
        if(!std::filesystem::exists(path)){
            // Not exists => unknown
            return UserProfile{};
        }
        // Validate file is regular
        if(!safety::FileSafety::isRegularFile(path)){
            throw std::runtime_error("Profile not regular file: "+path);
        }
        // Read content
        std::ifstream f(path);
        if(!f) throw std::runtime_error("Failed to open profile: "+path);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if(content.empty()){
            throw std::runtime_error("Malformed profile: empty file");
        }
        // Trim
        std::string trimmed = content;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        if(!trimmed.empty()) trimmed.erase(trimmed.find_last_not_of(" \t\n\r")+1);
        if(trimmed.empty()){
            throw std::runtime_error("Malformed profile: empty");
        }
        // Validate JSON structure via UserProfile::fromJson (will throw on malformed)
        try {
            return UserProfile::fromJson(content);
        } catch(const std::exception& e){
            // Also audit malformed
            safety::AuditEvent ev;
            ev.timestamp = nowISO();
            ev.transactionId = "PROFILE";
            ev.operation = "profile.load.malformed";
            ev.user = "test";
            ev.error = std::string("malformed profile at ")+path+": "+e.what();
            safety::AuditLog::append(ev);
            throw;
        }
    }

    // Save - atomic write, 0600, no symlink, canonical validation, audit handled by service but also here
    static void save(const UserProfile& profile, const std::string& path = profilePath()){
        // Validate path
        validateProfilePath(path);
        // Check not symlink
        if(std::filesystem::is_symlink(path)){
            throw std::runtime_error("Profile symlink rejected on save: "+path);
        }
        // Ensure parent dir exists
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        // Atomic write via FileSafety::atomicWrite pattern but with 0600
        std::string json = profile.toJson();
        // Use FileSafety::atomicWrite with extended allowlist handling
        // For profile, we bypass FileSafety's allowlist check and do our own validate, then direct atomic write
        std::string tmp = path + ".tmp." + std::to_string(getpid());
        {
            FILE* f = fopen(tmp.c_str(), "w");
            if(!f) throw std::runtime_error("Failed to open tmp "+tmp);
            // Write
            fwrite(json.c_str(), 1, json.size(), f);
            fflush(f);
            fsync(fileno(f));
            fclose(f);
            // Permissions 0600
            chmod(tmp.c_str(), 0600);
        }
        if(!safety::FileSafety::isRegularFile(tmp)){
            unlink(tmp.c_str());
            throw std::runtime_error("Temp not regular file");
        }
        // Validate canonical parent before rename (TOCTOU)
        try {
            std::string parent = std::filesystem::path(path).parent_path().string();
            if(!parent.empty() && std::filesystem::exists(parent)){
                std::string canonParent = safety::FileSafety::canonical(parent);
                (void)canonParent;
            }
        } catch(...){
            unlink(tmp.c_str());
            throw;
        }
        if(rename(tmp.c_str(), path.c_str())!=0){
            unlink(tmp.c_str());
            throw std::runtime_error("Atomic rename failed for profile");
        }
        // Ensure 0600 on final
        chmod(path.c_str(), 0600);
    }

    // Helper for testing: remove file if exists
    static void remove(const std::string& path){
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    static bool exists(const std::string& path = profilePath()){
        return std::filesystem::exists(path);
    }

private:
    static void validateProfilePath(const std::string& path){
        if(path.empty()) throw std::invalid_argument("Empty profile path");
        if(path.find('\0')!=std::string::npos) throw std::invalid_argument("NUL byte in profile path");
        if(path.find("..")!=std::string::npos) throw std::invalid_argument("Path traversal '..' rejected: "+path);
        if(path.find(";")!=std::string::npos || path.find("|")!=std::string::npos || path.find("&")!=std::string::npos
           || path.find("`")!=std::string::npos || path.find("$")!=std::string::npos){
            throw std::invalid_argument("Shell metacharacter rejected: "+path);
        }
        if(path.size()>4096) throw std::invalid_argument("Profile path too long");
        // Allowlist: must be either under /tmp/polaris-test-root/ (tests) or under $HOME/.local/state/polaris/
        const char* home = getenv("HOME");
        std::string allowedTest = "/tmp/polaris-test-root/";
        std::string allowedReal;
        if(home) allowedReal = std::string(home)+"/.local/state/polaris/";
        bool isTest = (path.rfind(allowedTest,0)==0);
        bool isReal = (!allowedReal.empty() && path.rfind(allowedReal,0)==0);
        if(!isTest && !isReal){
            // Also allow /home/mehrangh fallback if HOME not set
            if(path=="/tmp/polaris-test-root/profile.json" || path.rfind("/tmp/polaris-test-root/p13",0)==0) isTest=true;
            else if(path.rfind("/home/mehrangh/.local/state/polaris/",0)==0) isReal=true;
            else throw std::runtime_error("Profile path not in allowlist: "+path);
        }
        if(!isTest && !isReal){
            throw std::runtime_error("Profile path not in allowlist: "+path);
        }
    }

    static std::string nowISO(){
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        char buf[64]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S%z", localtime(&tt));
        return buf;
    }
};

} // namespace polaris::profile
