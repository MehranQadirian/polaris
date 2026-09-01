#include "../../core/domain/Comparison.h"
#include "../../core/engines/comparison/ComparisonEngine.h"
#include <cassert>
#include <iostream>

using namespace polaris::domain;
using namespace polaris::engines::comparison;

// Helper to make baseline with specific values
PerformanceBaseline makeBaseline(double userspace, double availMb, double swapMb, double thermal, int failed, bool nvidiaClaimed, const std::string& ts){
    PerformanceBaseline b;
    b.timestamp = ts;
    b.systemd.userspace = userspace;
    b.memory.availableKb = (uint64_t)(availMb * 1024);
    b.memory.swapUsedKb = (uint64_t)(swapMb * 1024);
    b.thermal.cpuMaxC = thermal;
    b.systemd.failedCount = failed;
    // For nvidia claimed, add gpu entry
    if(nvidiaClaimed || true){
        GpuBaseline::Gpu g;
        g.vendor="NVIDIA"; g.model="GM108M"; g.pci="01:00.0"; g.driver=nvidiaClaimed?"nvidia":""; g.claimed=nvidiaClaimed;
        b.gpu.gpus.push_back(g);
        // Also add Intel
        GpuBaseline::Gpu g2; g2.vendor="Intel"; g2.model="UHD"; g2.pci="00:02.0"; g2.driver="i915"; g2.claimed=true;
        b.gpu.gpus.push_back(g2);
    }
    return b;
}

void test_normal_improvement(){
    auto before = makeBaseline(54.106, 4.2*1024, 1.6*1024, 67, 1, false, "2026-08-31T21:09:00+0330");
    auto after = makeBaseline(8.515, 6.5*1024, 0, 50, 0, true, "2026-09-01T00:40:00+0330");
    auto c = ComparisonEngine::compare(before, after, "restore NVIDIA", "CMP-001");
    // Check boot delta -45.591 -84% not regression
    bool foundBoot=false;
    for(auto &m: c.metrics) if(m.metric=="boot.userspace"){
        assert(m.available);
        assert(m.delta.has_value());
        assert(*m.delta < 0); // improvement
        assert(!m.regression); // -84% not > +10%
        foundBoot=true;
    }
    assert(foundBoot);
    // Check nvidia claimed 0->1 => SUCCESS
    assert(c.verdict == Verdict::SUCCESS);
    assert(c.hasRegression==false);
    std::cout << "normal improvement PASS\n";
}

void test_unchanged_idempotent(){
    auto before = makeBaseline(8.515, 6.1*1024, 0, 50, 0, true, "2026-09-01T00:40:00+0330");
    auto after = makeBaseline(8.515, 6.1*1024, 0, 50, 0, true, "2026-09-01T00:41:00+0330");
    auto c = ComparisonEngine::compare(before, after, "no change", "CMP-002");
    assert(c.verdict == Verdict::NO_CHANGE);
    std::cout << "unchanged idempotent PASS\n";
}

void test_regression_boot(){
    auto before = makeBaseline(50, 8*1024, 0, 50, 0, true, "2026-08-31T00:00:00+0330");
    auto after = makeBaseline(70, 8*1024, 0, 50, 0, true, "2026-09-01T00:00:00+0330");
    auto c = ComparisonEngine::compare(before, after, "boot test", "CMP-003");
    for(auto &m: c.metrics) if(m.metric=="boot.userspace"){
        assert(m.regression); // +40% > +10%
    }
    assert(c.verdict == Verdict::REGRESSION);
    assert(c.hasRegression);
    std::cout << "regression boot PASS\n";
}

void test_regression_memory(){
    auto before = makeBaseline(8.515, 8*1024, 0, 50, 0, true, "2026-08-31T00:00:00+0330");
    auto after = makeBaseline(8.515, 6.5*1024, 0, 50, 0, true, "2026-09-01T00:00:00+0330");
    auto c = ComparisonEngine::compare(before, after, "mem test", "CMP-004");
    for(auto &m: c.metrics) if(m.metric=="memory.available"){
        assert(m.regression); // 1.5GB decrease >1GB
    }
    assert(c.hasRegression);
    std::cout << "regression memory PASS\n";
}

void test_regression_thermal(){
    auto before = makeBaseline(8.515, 8*1024, 0, 50, 0, true, "2026-08-31T00:00:00+0330");
    auto after = makeBaseline(8.515, 8*1024, 0, 70, 0, true, "2026-09-01T00:00:00+0330");
    auto c = ComparisonEngine::compare(before, after, "thermal test", "CMP-005");
    for(auto &m: c.metrics) if(m.metric=="thermal.cpuMax"){
        assert(m.regression); // +20C >15C
    }
    std::cout << "regression thermal PASS\n";
}

void test_new_failed_unit(){
    auto before = makeBaseline(8.515, 8*1024, 0, 50, 0, true, "2026-08-31T00:00:00+0330");
    auto after = makeBaseline(8.515, 8*1024, 0, 50, 1, true, "2026-09-01T00:00:00+0330");
    auto c = ComparisonEngine::compare(before, after, "failed test", "CMP-006");
    for(auto &m: c.metrics) if(m.metric=="systemd.failedCount"){
        assert(m.regression); // 0->1 new failed
    }
    std::cout << "new failed unit PASS\n";
}

void test_unavailable_metric(){
    PerformanceBaseline before, after;
    before.timestamp = "2026-08-31T00:00:00+0330";
    after.timestamp = "2026-09-01T00:00:00+0330";
    // Leave systemd.userspace as 0 (unavailable)
    before.systemd.userspace = 0;
    after.systemd.userspace = 0;
    auto c = ComparisonEngine::compare(before, after, "unavailable test", "CMP-007");
    for(auto &m: c.metrics) if(m.metric=="boot.userspace"){
        assert(!m.available);
        assert(m.note.find("unavailable")!=std::string::npos);
        assert(!m.regression);
    }
    std::cout << "unavailable metric PASS\n";
}

void test_zero_before(){
    auto before = makeBaseline(0, 8*1024, 0, 50, 0, true, "2026-08-31T00:00:00+0330");
    auto after = makeBaseline(10, 8*1024, 0, 50, 0, true, "2026-09-01T00:00:00+0330");
    // For boot 0->10, pctDelta should be 100% (handled), regression true because 100% >10%
    auto c = ComparisonEngine::compare(before, after, "zero before", "CMP-008");
    for(auto &m: c.metrics) if(m.metric=="boot.userspace"){
        // Before 0, after 10 => pct 100% => regression true
        // But our code handles zero-before as 100% if after !=0
        assert(m.regression);
    }
    std::cout << "zero-before PASS\n";
}

void test_threshold_boundary(){
    auto before = makeBaseline(50, 8*1024, 0, 50, 0, true, "2026-08-31T00:00:00+0330");
    auto after = makeBaseline(55, 8*1024, 0, 50, 0, true, "2026-09-01T00:00:00+0330"); // +10% exactly
    auto c = ComparisonEngine::compare(before, after, "boundary", "CMP-009");
    for(auto &m: c.metrics) if(m.metric=="boot.userspace"){
        // pct = (55-50)/50*100 = 10% exactly, threshold is >10%, so not regression (needs >)
        assert(!m.regression);
    }
    std::cout << "threshold boundary PASS\n";
}

void test_threshold_just_above(){
    auto before = makeBaseline(50, 8*1024, 0, 50, 0, true, "2026-08-31T00:00:00+0330");
    auto after = makeBaseline(55.5, 8*1024, 0, 50, 0, true, "2026-09-01T00:00:00+0330"); // +11%
    auto c = ComparisonEngine::compare(before, after, "just above", "CMP-010");
    for(auto &m: c.metrics) if(m.metric=="boot.userspace"){
        assert(m.regression); // 11% >10%
    }
    std::cout << "threshold just above PASS\n";
}

void test_expected_vs_observed(){
    auto before = makeBaseline(54.106, 4.2*1024, 1.6*1024, 67, 1, false, "2026-08-31T21:09:00+0330");
    auto after = makeBaseline(8.515, 6.5*1024, 0, 50, 0, true, "2026-09-01T00:40:00+0330");
    auto c = ComparisonEngine::compare(before, after, "restore NVIDIA", "CMP-011");
    assert(c.expectedBenefit=="restore NVIDIA");
    assert(c.observedBenefit.find("MX130 claimed")!=std::string::npos);
    assert(c.verdict==Verdict::SUCCESS);
    std::cout << "expected vs observed PASS\n";
}

void test_serialization(){
    auto before = makeBaseline(54.106, 4.2*1024, 0, 50, 0, false, "2026-08-31T21:09:00+0330");
    auto after = makeBaseline(8.515, 6.5*1024, 0, 50, 0, true, "2026-09-01T00:40:00+0330");
    auto c = ComparisonEngine::compare(before, after, "test", "CMP-012");
    // Simulate serialization: check deterministic
    auto c2 = ComparisonEngine::compare(before, after, "test", "CMP-012");
    assert(c.verdict==c2.verdict);
    assert(c.metrics.size()==c2.metrics.size());
    for(size_t i=0;i<c.metrics.size();i++){
        assert(c.metrics[i].metric==c2.metrics[i].metric);
        assert(c.metrics[i].regression==c2.metrics[i].regression);
    }
    std::cout << "serialization deterministic PASS\n";
}

void test_p7_fixture(){
    // P7 fixture from prompt: Before 54.106s avail 4.2GiB swap 1.6GiB thermal 67C, After 54.106s (but actually 8.515s boot, but P7 fixture says boot unchanged, avail 6.5GiB swap 0 thermal 50C)
    auto before = makeBaseline(54.106, 4.2*1024, 1.6*1024, 67, 1, false, "2026-08-31T21:09:00+0330");
    auto after = makeBaseline(54.106, 6.5*1024, 0, 50, 0, true, "2026-09-01T00:40:00+0330");
    auto c = ComparisonEngine::compare(before, after, "restore NVIDIA Maxwell support and PRIME offload", "CMP-P7");
    // Boot unchanged 54.106->54.106 => no regression, pct 0
    for(auto &m: c.metrics) if(m.metric=="boot.userspace") assert(!m.regression);
    // Memory avail increase 4.2->6.5 => not regression (increase is good, regression is decrease)
    for(auto &m: c.metrics) if(m.metric=="memory.available") assert(!m.regression);
    // Thermal 67->50 decrease => not regression
    for(auto &m: c.metrics) if(m.metric=="thermal.cpuMax") assert(!m.regression);
    // Failed 1->0 => not regression (decrease)
    for(auto &m: c.metrics) if(m.metric=="systemd.failedCount") assert(!m.regression);
    // Nvidia claimed 0->1 => not regression
    assert(c.verdict==Verdict::SUCCESS);
    std::cout << "P7 fixture PASS\n";
}

int main(){
    test_normal_improvement();
    test_unchanged_idempotent();
    test_regression_boot();
    test_regression_memory();
    test_regression_thermal();
    test_new_failed_unit();
    test_unavailable_metric();
    test_zero_before();
    test_threshold_boundary();
    test_threshold_just_above();
    test_expected_vs_observed();
    test_serialization();
    test_p7_fixture();
    std::cout << "All comparison tests PASS (12 categories)\n";
    return 0;
}
