#include "../../core/domain/Comparison.h"
#include "../../core/engines/comparison/ComparisonEngine.h"
#include <cassert>
#include <iostream>

using namespace polaris::domain;
using namespace polaris::engines::comparison;

int main(){
    auto make = [](double boot, double availGb, double thermal, int failed)->PerformanceBaseline{
        PerformanceBaseline b;
        b.timestamp = "2026-08-31T00:00:00+0330";
        b.systemd.userspace = boot;
        b.memory.availableKb = (uint64_t)(availGb*1024*1024);
        b.thermal.cpuMaxC = thermal;
        b.systemd.failedCount = failed;
        return b;
    };
    // Test regression thresholds relative to baseline, not global
    // 1. Boot regression > +10%
    {
        auto before = make(50, 8, 50, 0);
        auto after = make(70, 8, 50, 0);
        auto c = ComparisonEngine::compare(before, after, "test", "CMP-REG-001");
        assert(c.hasRegression);
        for(auto &m: c.metrics) if(m.metric=="boot.userspace") assert(m.regression);
        std::cout << "regression boot 50->70 +40% PASS\n";
    }
    // 2. Memory regression >1GiB decrease
    {
        auto before = make(50, 8, 50, 0);
        auto after = make(50, 6.5, 50, 0);
        auto c = ComparisonEngine::compare(before, after, "test", "CMP-REG-002");
        for(auto &m: c.metrics) if(m.metric=="memory.available") assert(m.regression);
        std::cout << "regression memory 8->6.5 -1.5GB PASS\n";
    }
    // 3. Thermal regression >+15C
    {
        auto before = make(50, 8, 50, 0);
        auto after = make(50, 8, 70, 0);
        auto c = ComparisonEngine::compare(before, after, "test", "CMP-REG-003");
        for(auto &m: c.metrics) if(m.metric=="thermal.cpuMax") assert(m.regression);
        std::cout << "regression thermal 50->70 +20C PASS\n";
    }
    // 4. Failed unit regression any new
    {
        auto before = make(50, 8, 50, 0);
        auto after = make(50, 8, 50, 1);
        auto c = ComparisonEngine::compare(before, after, "test", "CMP-REG-004");
        for(auto &m: c.metrics) if(m.metric=="systemd.failedCount") assert(m.regression);
        std::cout << "regression failed 0->1 PASS\n";
    }
    // 5. No regression when within thresholds
    {
        auto before = make(50, 8, 50, 0);
        auto after = make(54, 7.5, 60, 0); // boot +8% (<10%), mem -0.5GB (<1), thermal +10 (<15)
        auto c = ComparisonEngine::compare(before, after, "test", "CMP-REG-005");
        assert(!c.hasRegression);
        std::cout << "no regression within thresholds PASS\n";
    }
    std::cout << "All regression tests PASS\n";
    return 0;
}
