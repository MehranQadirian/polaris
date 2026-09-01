#include "BackupEngine.h"
#include "../FileSafety.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <chrono>
#include <ctime>

namespace polaris::safety {

std::string BackupEngine::sha256File(const std::string& path){
    std::ifstream f(path, std::ios::binary);
    if(!f) return "";
    SHA256_CTX ctx; SHA256_Init(&ctx);
    char buf[8192];
    while(f.read(buf, sizeof(buf)) || f.gcount()){
        SHA256_Update(&ctx, buf, f.gcount());
    }
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);
    std::ostringstream oss;
    for(int i=0;i<SHA256_DIGEST_LENGTH;i++) oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

Backup BackupEngine::create(const std::string& transactionId, const std::string& originalPath){
    // Validate allowlist via FileSafety (P4 only test root)
    FileSafety::validatePath(originalPath);
    if(!FileSafety::isRegularFile(originalPath)){
        throw std::runtime_error("Backup source not regular file: "+originalPath);
    }
    std::string root = backupRoot();
    // For P4 tests, use testBackupRoot if original is under test root
    if(originalPath.rfind("/tmp/polaris-test-root/",0)==0) root = testBackupRoot();
    std::string backupDir = root + "/" + transactionId;
    std::filesystem::create_directories(backupDir);
    std::string filename = std::filesystem::path(originalPath).filename().string();
    std::string backupPath = backupDir + "/" + filename + ".bak";

    if(std::filesystem::exists(backupPath)){
        throw std::runtime_error("Backup already exists, refusing to overwrite: "+backupPath);
    }

    // Copy file
    std::ifstream src(originalPath, std::ios::binary);
    std::ofstream dst(backupPath, std::ios::binary);
    dst << src.rdbuf();
    src.close(); dst.close();

    // Metadata
    struct stat st;
    stat(originalPath.c_str(), &st);
    std::string perms;
    {
        std::ostringstream oss;
        oss << std::oct << (st.st_mode & 0777);
        perms = oss.str();
    }
    struct passwd* pw = getpwuid(st.st_uid);
    struct group* gr = getgrgid(st.st_gid);
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    char buf[64]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S%z", localtime(&tt));

    Backup b;
    b.transactionId = transactionId;
    b.originalPath = originalPath;
    b.backupPath = backupPath;
    b.timestamp = buf;
    b.sha256 = sha256File(originalPath);
    b.size = st.st_size;
    b.permissions = perms;
    b.owner = pw ? pw->pw_name : std::to_string(st.st_uid);
    b.group = gr ? gr->gr_name : std::to_string(st.st_gid);
    return b;
}

bool BackupEngine::restore(const Backup& b){
    // Validate current state not unexpectedly changed? For P4 we just restore
    if(!std::filesystem::exists(b.backupPath)) return false;
    // Ensure original path still under test root
    FileSafety::validatePath(b.originalPath);
    std::ifstream src(b.backupPath, std::ios::binary);
    std::ofstream dst(b.originalPath, std::ios::binary);
    dst << src.rdbuf();
    return true;
}

} // namespace polaris::safety
