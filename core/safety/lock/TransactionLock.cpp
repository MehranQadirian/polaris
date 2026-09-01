#include "TransactionLock.h"
#include "../audit/AuditLog.h"
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <chrono>
#include <ctime>

namespace polaris::safety {

TransactionLock::TransactionLock(const std::string& lockPath) : lockPath_(lockPath) {}
TransactionLock::~TransactionLock(){ unlock(); }

std::string TransactionLock::testLockPath(const std::string& dir){
    if(dir.empty()) return testLockPath();
    if(dir.back()=='/') return dir + "transaction.lock";
    return dir + "/transaction.lock";
}

std::string TransactionLock::nowISO(){
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    char buf[64]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S%z", localtime(&tt));
    return buf;
}
void TransactionLock::audit(const std::string& op, const std::string& detail){
    AuditEvent ev;
    ev.timestamp = nowISO();
    ev.transactionId = "LOCK";
    ev.operation = op;
    ev.user = "test";
    ev.error = detail;
    AuditLog::append(ev);
}

bool TransactionLock::tryLock(){
    if(locked_) return true;
    // Ensure parent dir exists, not symlink
    std::string parent = std::filesystem::path(lockPath_).parent_path().string();
    if(!parent.empty()){
        std::error_code ec;
        if(std::filesystem::is_symlink(parent, ec) && !ec){
            audit("lock.rejected", "parent is symlink: "+parent);
            return false;
        }
        std::filesystem::create_directories(parent);
        // Not world-writable check
        struct stat st;
        if(stat(parent.c_str(), &st)==0){
            if(st.st_mode & S_IWOTH){
                audit("lock.rejected", "parent world-writable");
                return false;
            }
        }
    }
    // Check lock file not symlink
    std::error_code ec;
    if(std::filesystem::is_symlink(lockPath_, ec) && !ec){
        audit("lock.rejected", "lock path is symlink");
        return false;
    }
    fd_ = open(lockPath_.c_str(), O_CREAT|O_RDWR, 0600);
    if(fd_<0){
        audit("lock.rejected", "open failed");
        return false;
    }
    // FD_CLOEXEC
    fcntl(fd_, F_SETFD, FD_CLOEXEC);
    // Try exclusive non-blocking
    if(flock(fd_, LOCK_EX|LOCK_NB)!=0){
        close(fd_); fd_=-1;
        audit("lock.rejected", "contention: already locked");
        return false;
    }
    // Ensure not world-writable file
    chmod(lockPath_.c_str(), 0600);
    locked_=true;
    audit("lock.acquire", "acquired "+lockPath_);
    return true;
}

bool TransactionLock::unlock(){
    if(!locked_) {
        if(fd_>=0){ close(fd_); fd_=-1; }
        return true;
    }
    if(fd_>=0){
        flock(fd_, LOCK_UN);
        close(fd_);
        fd_=-1;
    }
    locked_=false;
    audit("lock.release", "released "+lockPath_);
    return true;
}

} // namespace polaris::safety
