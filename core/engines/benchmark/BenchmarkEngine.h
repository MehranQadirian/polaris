#pragma once
#include "../../domain/PerfModels.h"
#include <vector>
#include <string>
#include <functional>

namespace polaris::engines::benchmark {

class BenchmarkEngine {
public:
    enum class Mode { QUICK, NORMAL, DEEP };

    // All read-only, non-destructive, cancellable, time-limited, thermally aware
    static std::vector<domain::BenchmarkResult> run(Mode mode, int runs=3);

    static std::string modeToString(Mode m){
        if(m==Mode::QUICK) return "quick";
        if(m==Mode::NORMAL) return "normal";
        return "deep";
    }
    static std::string expectedLoad(Mode m){
        if(m==Mode::QUICK) return "CPU <10% for 2s, no I/O, temp +2C";
        if(m==Mode::NORMAL) return "CPU 30% for 5s, light I/O, temp +5C";
        return "CPU 70% for 10s, moderate I/O, temp +10C - approval recommended";
    }

private:
    static domain::BenchmarkResult benchCpuQuick(int runs);
    static domain::BenchmarkResult benchMemQuick(int runs);
    static domain::BenchmarkResult benchProcList(int runs);
};

} // namespace polaris::engines::benchmark
