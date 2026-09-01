#include "../../core/domain/Comparison.h"
#include "../../core/engines/comparison/ComparisonEngine.h"
#include <cassert>
#include <iostream>

using namespace polaris::domain;
using namespace polaris::engines::comparison;

int main(){
    auto make = [](double boot, double availGb, double thermal, int failed, bool nvidiaClaimed)->PerformanceBaseline{
        PerformanceBaseline b;
        b.timestamp = "2026-08-31T00:00:00+0330";
        b.systemd.userspace = boot;
        b.memory.availableKb = (uint64_t)(availGb*1024*1024);
        b.thermal.cpuMaxC = thermal;
        b.systemd.failedCount = failed;
        GpuBaseline::Gpu g; g.vendor="NVIDIA"; g.claimed=nvidiaClaimed; b.gpu.gpus.push_back(g);
        GpuBaseline::Gpu g2; g2.vendor="Intel"; g2.claimed=true; b.gpu.gpus.push_back(g2);
        return b;
    };

    // Test expected vs observed benefit
    // 1. Expected NVIDIA functional, observed achieved
    {
        auto before = make(54.106, 4.2, 67, 1, false);
        auto after = make(8.515, 6.5, 50, 0, true);
        auto c = ComparisonEngine::compare(before, after, "restore NVIDIA Maxwell support and PRIME offload", "CMP-OBS-001");
        assert(c.expectedBenefit=="restore NVIDIA Maxwell support and PRIME offload");
        assert(c.observedBenefit.find("MX130 claimed")!=std::string::npos);
        assert(c.verdict==Verdict::SUCCESS);
        // Must not claim success merely because command completed - here observed matches expected and no regression
        assert(!c.hasRegression);
        std::cout << "expected vs observed SUCCESS PASS\n";
    }
    // 2. Operation succeeded but no benefit (e.g., boot unchanged, memory same, nvidia still not claimed)
    {
        auto before = make(50, 8, 50, 0, false);
        auto after = make(50, 8, 50, 0, false);
        auto c = ComparisonEngine::compare(before, after, "restore NVIDIA", "CMP-OBS-002");
        // No nvidia improvement, no regression, but also no benefit
        assert(c.verdict==Verdict::NO_CHANGE || c.verdict==Verdict::NO_BENEFIT);
        std::cout << "operation succeeded but no benefit PASS (verdict " << toString(c.verdict) << ")\n";
    }
    // 3. Separate operation succeeded vs verification succeeded vs benefit observed vs regression
    {
        auto before = make(50, 8, 50, 0, false);
        auto after = make(50, 8, 50, 1, false); // new failed unit, so regression even though operation completed
        auto c = ComparisonEngine::compare(before, after, "test", "CMP-OBS-003");
        assert(c.hasRegression);
        assert(c.verdict==Verdict::REGRESSION);
        // Even though operation completed, verification should fail due to regression
        std::cout << "operation succeeded but verification failed due to regression PASS\n";
    }
    std::cout << "All observed benefit tests PASS\n";
    return 0;
}
