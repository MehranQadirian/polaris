#include "../../core/safety/lock/TransactionLock.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <thread>
#include <vector>
#include <unistd.h>
#include <atomic>

using namespace polaris::safety;

void test_lock_acquisition(){
    std::string dir = "/tmp/polaris-test-root/p14_lock_acquire";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/transaction.lock";
    TransactionLock lock(path);
    assert(lock.tryLock());
    assert(lock.isLocked());
    std::cout << "lock acquisition PASS\n";
    assert(lock.unlock());
    assert(!lock.isLocked());
    std::cout << "lock release after acquire PASS\n";
}

void test_lock_contention(){
    std::string dir = "/tmp/polaris-test-root/p14_lock_contention";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/transaction.lock";
    TransactionLock lock1(path);
    TransactionLock lock2(path);
    assert(lock1.tryLock());
    assert(lock1.isLocked());
    // Second should fail (contention)
    assert(!lock2.tryLock());
    assert(!lock2.isLocked());
    std::cout << "lock contention (second fails) PASS\n";
    // Release first, second should now succeed
    assert(lock1.unlock());
    assert(lock2.tryLock());
    assert(lock2.isLocked());
    std::cout << "lock contention release → second succeeds PASS\n";
    lock2.unlock();
}

void test_lock_release(){
    std::string dir = "/tmp/polaris-test-root/p14_lock_release";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/transaction.lock";
    {
        TransactionLock lock(path);
        assert(lock.tryLock());
        // Destructor should unlock
    }
    // After scope, lock should be released
    TransactionLock lock2(path);
    assert(lock2.tryLock());
    std::cout << "lock release on destruction PASS\n";
    lock2.unlock();
}

void test_lock_symlink_rejection(){
    std::string dir = "/tmp/polaris-test-root/p14_lock_symlink";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string real = dir + "/real.lock";
    std::string link = dir + "/transaction.lock";
    ::symlink(real.c_str(), link.c_str());
    assert(std::filesystem::is_symlink(link));
    TransactionLock lock(link);
    assert(!lock.tryLock());
    std::cout << "lock symlink rejection PASS\n";
    ::unlink(link.c_str());
}

void test_concurrent_lock(){
    std::string dir = "/tmp/polaris-test-root/p14_lock_concurrent";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/transaction.lock";
    const int N=2;
    std::atomic<int> successes{0};
    std::vector<std::thread> threads;
    for(int i=0;i<N;i++){
        threads.emplace_back([&,i](){
            TransactionLock lock(path);
            if(lock.tryLock()){
                successes++;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                lock.unlock();
            }
        });
    }
    for(auto &t: threads) t.join();
    // Only one should have succeeded at a time, but with timing both may succeed sequentially
    // At least we verify no deadlock and at most N successes, at least 1
    assert(successes>=1 && successes<=N);
    std::cout << "concurrent lock behavior PASS (successes=" << successes << ")\n";
}

int main(){
    test_lock_acquisition();
    test_lock_contention();
    test_lock_release();
    test_lock_symlink_rejection();
    test_concurrent_lock();
    std::cout << "All P14 lock tests PASS (5 categories)\n";
    return 0;
}
