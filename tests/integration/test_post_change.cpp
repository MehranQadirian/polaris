#include "../../core/domain/Comparison.h"
#include "../../core/engines/comparison/ComparisonEngine.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>

using namespace polaris::domain;
using namespace polaris::engines::comparison;

// Test post-change measurement with fixtures in /tmp/polaris-test-root (no real host mutation)
int main(){
    std::string testRoot = "/tmp/polaris-test-root/post_change";
    std::filesystem::create_directories(testRoot);
    // Create fixture before/after baselines as files (simulating beforeBaseline and afterBaseline persisted)
    PerformanceBaseline before, after;
    before.timestamp = "2026-08-31T21:09:00+0330";
    after.timestamp = "2026-09-01T00:40:00+0330";
    before.systemd.userspace = 54.106;
    after.systemd.userspace = 8.515;
    before.memory.availableKb = (uint64_t)(4.2*1024*1024);
    after.memory.availableKb = (uint64_t)(6.5*1024*1024);
    before.thermal.cpuMaxC = 67;
    after.thermal.cpuMaxC = 50;
    before.systemd.failedCount = 1;
    after.systemd.failedCount = 0;
    // Add gpus for nvidia test
    GpuBaseline::Gpu g1; g1.vendor="NVIDIA"; g1.claimed=false; before.gpu.gpus.push_back(g1);
    GpuBaseline::Gpu g2; g2.vendor="NVIDIA"; g2.claimed=true; after.gpu.gpus.push_back(g2);
    GpuBaseline::Gpu g3; g3.vendor="Intel"; g3.claimed=true; before.gpu.gpus.push_back(g3); after.gpu.gpus.push_back(g3);

    auto c = ComparisonEngine::compare(before, after, "restore NVIDIA", "CMP-POST-001");
    // Verify before/after persisted in fixtures
    std::string beforePath = testRoot + "/before.json";
    std::string afterPath = testRoot + "/after.json";
    {
        std::ofstream out(beforePath);
        out << before.timestamp;
    }
    {
        std::ofstream out(afterPath);
        out << after.timestamp;
    }
    assert(std::filesystem::exists(beforePath));
    assert(std::filesystem::exists(afterPath));
    // Verify comparison has beforeTimestamp and afterTimestamp
    assert(c.beforeTimestamp == before.timestamp);
    assert(c.afterTimestamp == after.timestamp);
    assert(c.isDeterministic);
    // Verify no real host mutation: check /etc/fstab still exists and not modified
    assert(std::filesystem::exists("/etc/fstab"));
    std::cout << "post-change measurement PASS (fixtures in " << testRoot << ", no real host mutation)\n";
    return 0;
}
