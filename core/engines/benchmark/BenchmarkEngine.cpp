#include "BenchmarkEngine.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <unistd.h>
#include <sys/statvfs.h>

namespace polaris::engines::benchmark {

std::vector<domain::BenchmarkResult> BenchmarkEngine::run(Mode mode, int runs) {
    std::vector<domain::BenchmarkResult> out;
    if(mode==Mode::QUICK){
        out.push_back(benchCpuQuick(runs));
        out.push_back(benchMemQuick(runs));
        out.push_back(benchProcList(runs));
    } else if(mode==Mode::NORMAL){
        out.push_back(benchCpuQuick(runs));
        out.push_back(benchMemQuick(runs*2));
        out.push_back(benchProcList(runs));
        // Add storage statvfs bench (read-only)
        domain::BenchmarkResult s;
        s.mode="normal"; s.name="statvfs"; s.unit="ms"; s.runs=runs;
        std::vector<double> vals;
        for(int i=0;i<runs;i++){
            auto t0=std::chrono::steady_clock::now();
            struct statvfs sv; statvfs("/", &sv);
            auto t1=std::chrono::steady_clock::now();
            double ms=std::chrono::duration<double, std::milli>(t1-t0).count();
            vals.push_back(ms);
        }
        s.min=*std::min_element(vals.begin(), vals.end());
        s.max=*std::max_element(vals.begin(), vals.end());
        s.avg=std::accumulate(vals.begin(), vals.end(), 0.0)/vals.size();
        std::sort(vals.begin(), vals.end());
        s.median=vals[vals.size()/2];
        double var=0; for(auto v: vals) var+=(v-s.avg)*(v-s.avg); s.stddev=std::sqrt(var/vals.size());
        s.meta={"", "ms", "statvfs", "native", 0.90f, true, ""};
        out.push_back(s);
    } else { // DEEP - still read-only but heavier, report expected load
        out.push_back(benchCpuQuick(runs*2));
        out.push_back(benchMemQuick(runs*3));
        // For DEEP, add repeated thermal read + journal count
        domain::BenchmarkResult j;
        j.mode="deep"; j.name="journal_count"; j.unit="ms"; j.runs=runs;
        std::vector<double> vals;
        for(int i=0;i<runs;i++){
            auto t0=std::chrono::steady_clock::now();
            // read journal count via fixed exec (already covered) - here just simulate via reading /proc
            std::ifstream f("/proc/loadavg"); std::string l; std::getline(f,l);
            auto t1=std::chrono::steady_clock::now();
            vals.push_back(std::chrono::duration<double, std::milli>(t1-t0).count());
        }
        j.min=*std::min_element(vals.begin(), vals.end());
        j.max=*std::max_element(vals.begin(), vals.end());
        j.avg=std::accumulate(vals.begin(), vals.end(), 0.0)/vals.size();
        j.meta={"", "ms", "/proc/loadavg", "procfs", 0.85f, true, ""};
        out.push_back(j);
    }
    return out;
}

domain::BenchmarkResult BenchmarkEngine::benchCpuQuick(int runs){
    domain::BenchmarkResult r;
    r.mode="quick"; r.name="cpu_prime"; r.unit="ms"; r.runs=runs;
    std::vector<double> vals;
    for(int i=0;i<runs;i++){
        auto t0=std::chrono::steady_clock::now();
        // Simple prime compute (non-destructive, CPU light)
        volatile long sum=0;
        for(int n=2; n<2000; n++){
            bool prime=true;
            for(int d=2; d*d<=n; d++) if(n%d==0){ prime=false; break; }
            if(prime) sum+=n;
        }
        (void)sum;
        auto t1=std::chrono::steady_clock::now();
        vals.push_back(std::chrono::duration<double, std::milli>(t1-t0).count());
    }
    r.min=*std::min_element(vals.begin(), vals.end());
    r.max=*std::max_element(vals.begin(), vals.end());
    r.avg=std::accumulate(vals.begin(), vals.end(), 0.0)/vals.size();
    std::sort(vals.begin(), vals.end());
    r.median=vals[vals.size()/2];
    double var=0; for(auto v: vals) var+=(v-r.avg)*(v-r.avg); r.stddev=std::sqrt(var/vals.size());
    r.meta={"", "ms", "prime compute 2..2000", "cpu", 0.85f, true, ""};
    return r;
}

domain::BenchmarkResult BenchmarkEngine::benchMemQuick(int runs){
    domain::BenchmarkResult r;
    r.mode="quick"; r.name="mem_read"; r.unit="ms"; r.runs=runs;
    std::vector<double> vals;
    for(int i=0;i<runs;i++){
        auto t0=std::chrono::steady_clock::now();
        // Read /proc/meminfo repeatedly (read-only, no alloc stress)
        for(int k=0;k<100;k++){
            std::ifstream f("/proc/meminfo");
            std::string l; std::getline(f,l);
        }
        auto t1=std::chrono::steady_clock::now();
        vals.push_back(std::chrono::duration<double, std::milli>(t1-t0).count());
    }
    r.min=*std::min_element(vals.begin(), vals.end());
    r.max=*std::max_element(vals.begin(), vals.end());
    r.avg=std::accumulate(vals.begin(), vals.end(), 0.0)/vals.size();
    std::sort(vals.begin(), vals.end());
    r.median=vals[vals.size()/2];
    double var=0; for(auto v: vals) var+=(v-r.avg)*(v-r.avg); r.stddev=std::sqrt(var/vals.size());
    r.meta={"", "ms", "/proc/meminfo x100", "procfs", 0.85f, true, ""};
    return r;
}

domain::BenchmarkResult BenchmarkEngine::benchProcList(int runs){
    domain::BenchmarkResult r;
    r.mode="quick"; r.name="proc_list"; r.unit="ms"; r.runs=runs;
    std::vector<double> vals;
    for(int i=0;i<runs;i++){
        auto t0=std::chrono::steady_clock::now();
        // List /proc (read-only)
        for(int k=0;k<10;k++){
            // Simulate via opendir
            // We avoid actual opendir here to keep deterministic - just measure getload
            std::ifstream f("/proc/loadavg"); std::string l; std::getline(f,l);
        }
        auto t1=std::chrono::steady_clock::now();
        vals.push_back(std::chrono::duration<double, std::milli>(t1-t0).count());
    }
    r.min=*std::min_element(vals.begin(), vals.end());
    r.max=*std::max_element(vals.begin(), vals.end());
    r.avg=std::accumulate(vals.begin(), vals.end(), 0.0)/vals.size();
    std::sort(vals.begin(), vals.end());
    r.median=vals[vals.size()/2];
    double var=0; for(auto v: vals) var+=(v-r.avg)*(v-r.avg); r.stddev=std::sqrt(var/vals.size());
    r.meta={"", "ms", "/proc/loadavg x10", "procfs", 0.85f, true, ""};
    return r;
}

} // namespace polaris::engines::benchmark
