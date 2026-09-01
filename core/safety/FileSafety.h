#pragma once
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <filesystem>
#include <unistd.h>

namespace polaris::safety {

// P4 File Safety: canonical validation, allowlist, symlink protection, etc.
// Operates only on test fixtures in P4: /tmp/polaris-test-root is allowed.
// Real host paths (/etc) are allowlisted but P4 will reject real writes - only dry-run.

class FileSafety {
public:
    // Allowlist for P4/P13: test fixtures + explicit safe real paths (but P4 will reject real writes - only dry-run, P13 profile is safe user file)
    static bool isAllowedPath(const std::string& path){
        const std::vector<std::string> allow = {
            "/tmp/polaris-test-root/",
            "/tmp/polaris-test-root/etc/fstab",
            "/tmp/polaris-test-root/etc/test.conf",
            "/etc/fstab",
            "/etc/default/grub",
            "/etc/modprobe.d/",
            "/home/mehrangh/.config/kwinrc",
            "/home/mehrangh/.config/autostart/nvidia-settings-user.desktop", // P5 pilot real host, user-owned, R1
        };
        for(auto &a: allow){
            if(path.rfind(a,0)==0) return true;
            if(path==a) return true;
        }
        // Also allow dynamic home for P5 and P13 profile
        const char* home = getenv("HOME");
        if(home){
            std::string p5 = std::string(home) + "/.config/autostart/nvidia-settings-user.desktop";
            if(path==p5) return true;
            std::string profile = std::string(home) + "/.local/state/polaris/profile.json";
            if(path==profile) return true;
            std::string profileDir = std::string(home) + "/.local/state/polaris/";
            if(path.rfind(profileDir,0)==0) return true;
        }
        // Fallback for test
        if(path=="/home/mehrangh/.local/state/polaris/profile.json") return true;
        if(path.rfind("/home/mehrangh/.local/state/polaris/",0)==0) return true;
        return false;
    }

    static void validatePath(const std::string& path){
        if(path.empty()) throw std::invalid_argument("Empty path");
        if(path.find('\0')!=std::string::npos) throw std::invalid_argument("NUL byte in path");
        if(path.find("..")!=std::string::npos) throw std::invalid_argument("Path traversal '..' rejected: "+path);
        if(path.find(";")!=std::string::npos || path.find("|")!=std::string::npos || path.find("&")!=std::string::npos
           || path.find("`")!=std::string::npos || path.find("$")!=std::string::npos){
            throw std::invalid_argument("Shell metacharacter rejected: "+path);
        }
        if(path.size()>4096) throw std::invalid_argument("Path too long");
        // P4/P13: only test fixtures; P5: allow single real host user file (R1); P13: profile
        const char* home = getenv("HOME");
        std::string p5 = home ? std::string(home)+"/.config/autostart/nvidia-settings-user.desktop" : "/home/mehrangh/.config/autostart/nvidia-settings-user.desktop";
        bool isP5Real = (path==p5);
        std::string profile = home ? std::string(home)+"/.local/state/polaris/profile.json" : "/home/mehrangh/.local/state/polaris/profile.json";
        bool isProfile = (path==profile);
        std::string profileDir = home ? std::string(home)+"/.local/state/polaris/" : "/home/mehrangh/.local/state/polaris/";
        bool isProfileDir = (path.rfind(profileDir,0)==0);
        if(path.rfind("/tmp/polaris-test-root/",0)!=0 && !isP5Real && !isProfile && !isProfileDir){
            // Also allow fallback test profile dir
            if(path.rfind("/home/mehrangh/.local/state/polaris/",0)==0) {
                // allow
            } else {
                throw std::runtime_error("READ-ONLY: Real host path rejected for mutation (only test fixtures and P5 pilot and P13 profile allowed): "+path);
            }
        }
    }

    static bool isRegularFile(const std::string& path){
        std::filesystem::path p(path);
        std::error_code ec;
        return std::filesystem::is_regular_file(p, ec);
    }

    static bool isSymlink(const std::string& path){
        std::filesystem::path p(path);
        std::error_code ec;
        return std::filesystem::is_symlink(p, ec);
    }

    static std::string canonical(const std::string& path){
        std::error_code ec;
        auto cp = std::filesystem::canonical(path, ec);
        if(ec) throw std::runtime_error("Canonical failed for "+path+": "+ec.message());
        return cp.string();
    }

    // Atomic write: write to temp + fsync + rename - only for test fixtures in P4
    static void atomicWrite(const std::string& path, const std::string& content){
        validatePath(path);
        if(isSymlink(path)) throw std::runtime_error("Symlink attack rejected: "+path);
        std::string tmp = path + ".tmp." + std::to_string(getpid());
        {
            std::filesystem::create_directories(std::filesystem::path(path).parent_path());
            FILE* f = fopen(tmp.c_str(), "w");
            if(!f) throw std::runtime_error("Failed to open tmp "+tmp);
            fwrite(content.c_str(), 1, content.size(), f);
            fflush(f);
            fsync(fileno(f));
            fclose(f);
        }
        // Validate temp is regular file
        if(!isRegularFile(tmp)) throw std::runtime_error("Temp not regular file");
        if(rename(tmp.c_str(), path.c_str())!=0){
            unlink(tmp.c_str());
            throw std::runtime_error("Atomic rename failed");
        }
    }
};

} // namespace polaris::safety
