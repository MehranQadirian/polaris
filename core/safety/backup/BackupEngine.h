#pragma once
#include "../transaction/Transaction.h"
#include <string>
#include <vector>
#include <filesystem>

namespace polaris::safety {

struct Backup {
    std::string transactionId;
    std::string originalPath; // e.g., /tmp/polaris-test-root/etc/fstab
    std::string backupPath; // ~/.local/state/polaris/backups/<tx>/fstab.bak
    std::string timestamp;
    std::string sha256;
    size_t size=0;
    std::string permissions; // e.g., 0644
    std::string owner, group;
};

class BackupEngine {
public:
    // Creates versioned backup in test root or state dir - does not overwrite existing
    static Backup create(const std::string& transactionId, const std::string& originalPath);

    static bool restore(const Backup& b);

    static std::string sha256File(const std::string& path);

    static std::string backupRoot(){
        const char* home = getenv("HOME");
        std::string base = home ? std::string(home)+"/.local/state/polaris/backups" : "/tmp/polaris-backups";
        return base;
    }
    static std::string testBackupRoot(){
        return "/tmp/polaris-test-root/backups";
    }
};

} // namespace polaris::safety
