#pragma once
#include <string>
#include <optional>
#include <filesystem>

namespace polaris::safety {

class TransactionLock {
public:
    explicit TransactionLock(const std::string& lockPath = defaultLockPath());
    ~TransactionLock();

    static std::string defaultLockPath(){ return "/run/polaris/transaction.lock"; }
    static std::string testLockPath(){ return "/tmp/polaris-test-root/p14/transaction.lock"; }
    static std::string testLockPath(const std::string& dir);

    // Try to acquire exclusive lock non-blocking. Returns true on success, false on contention.
    // Fail closed if cannot acquire.
    bool tryLock();
    bool isLocked() const { return locked_; }
    bool unlock();
    std::string path() const { return lockPath_; }

    // Non-copyable
    TransactionLock(const TransactionLock&) = delete;
    TransactionLock& operator=(const TransactionLock&) = delete;

private:
    std::string lockPath_;
    int fd_ = -1;
    bool locked_ = false;
    static std::string nowISO();
    void audit(const std::string& op, const std::string& detail);
};

} // namespace polaris::safety
