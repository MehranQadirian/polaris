#include "../core/engines/perf/BaselineEngine.h"
#include "../core/engines/bottleneck/BottleneckEngine.h"
#include "../core/engines/benchmark/BenchmarkEngine.h"
#include "../core/engines/recommend/RecommendationEngine.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <fstream>

std::string json_escape(const std::string& s){
    std::string out;
    for(char c: s){
        if(c=='"') out+="\\\"";
        else if(c=='\\') out+="\\\\";
        else if(c=='\n') out+="\\n";
        else if(c=='\r') out+="\\r";
        else if(c=='\t') out+="\\t";
        else if((unsigned char)c<0x20){ char buf[7]; snprintf(buf,sizeof(buf),"\\u%04x",(unsigned)c); out+=buf; }
        else out+=c;
    }
    return out;
}

void outputBaselineJson(const polaris::domain::PerformanceBaseline& b, bool pretty){
    // Use manual JSON for P3 without external lib
    std::ostringstream j;
    j << "{\n";
    j << "  \"id\":\"" << json_escape(b.id) << "\",\n";
    j << "  \"timestamp\":\"" << json_escape(b.timestamp) << "\",\n";
    j << "  \"cpu\":{\"model\":\"" << json_escape(b.cpu.model) << "\",\"cores\":" << b.cpu.cores << ",\"threads\":" << b.cpu.threads
      << ",\"governor\":\"" << json_escape(b.cpu.governor) << "\",\"epp\":\"" << json_escape(b.cpu.epp) << "\",\"curMhz\":" << b.cpu.curMhz << ",\"pressureSome10\":" << b.cpu.pressureSome10 << ",\"thermalMaxC\":" << b.cpu.thermalMaxC << "},\n";
    j << "  \"memory\":{\"totalKb\":" << b.memory.totalKb << ",\"availableKb\":" << b.memory.availableKb << ",\"swapUsedKb\":" << b.memory.swapUsedKb << ",\"zramData\":" << b.memory.zramData << ",\"pressureSome10\":" << b.memory.pressureSome10 << "},\n";
    j << "  \"systemd\":{\"firmware\":" << b.systemd.firmware << ",\"loader\":" << b.systemd.loader << ",\"kernel\":" << b.systemd.kernel << ",\"initrd\":" << b.systemd.initrd << ",\"userspace\":" << b.systemd.userspace << ",\"failedCount\":" << b.systemd.failedCount << "},\n";
    j << "  \"thermal\":{\"cpuMaxC\":" << b.thermal.cpuMaxC << ",\"throttling\":" << (b.thermal.throttling?"true":"false") << "},\n";
    j << "  \"journal\":{\"p3count\":" << b.journal.p3count << ",\"nvidiaErrs\":" << b.journal.nvidiaErrs << "}\n";
    j << "}\n";
    if(pretty) std::cout << j.str();
    else std::cout << j.str();
}

int main(int argc, char** argv){
    using namespace polaris::engines::perf;
    using namespace polaris::engines::bottleneck;
    using namespace polaris::engines::benchmark;
    using namespace polaris::engines::recommend;

    std::string cmd = argc>1? argv[1] : "analyze";
    std::string sub = argc>2? argv[2] : "";
    bool jsonMode = false; bool human=true;
    for(int i=1;i<argc;i++){ std::string a=argv[i]; if(a=="--json") { jsonMode=true; human=false; } if(a=="--human") human=true; }
    if(!jsonMode && cmd=="analyze") human=true;
    (void)human;

    auto t0 = std::chrono::steady_clock::now();
    auto baseline = BaselineEngine::collect();
    auto t1 = std::chrono::steady_clock::now();
    float baselineMs = std::chrono::duration<float, std::milli>(t1-t0).count();

    auto bottlenecks = BottleneckEngine::analyze(baseline);
    auto benchmarksQuick = BenchmarkEngine::run(BenchmarkEngine::Mode::QUICK, 3);
    // For P3, also run normal for comparison but not deep (requires approval)
    // Keep quick only for default to avoid load

    auto recs = RecommendationEngine::generate(baseline, bottlenecks);

    if(cmd=="performance" && sub=="baseline"){
        if(jsonMode){
            outputBaselineJson(baseline, true);
        } else {
            std::cout << "# P3 Baseline Human\n";
            std::cout << "Timestamp: " << baseline.timestamp << "\n";
            std::cout << "CPU: " << baseline.cpu.model << " " << baseline.cpu.cores << "C/" << baseline.cpu.threads << "T gov " << baseline.cpu.governor << " epp " << baseline.cpu.epp << " cur " << baseline.cpu.curMhz << "MHz\n";
            std::cout << "Memory: total " << baseline.memory.totalKb/1024 << "MB avail " << baseline.memory.availableKb/1024 << "MB swapUsed " << baseline.memory.swapUsedKb/1024 << "MB zramData " << baseline.memory.zramData/1024/1024 << "MB pressure " << baseline.memory.pressureSome10 << "\n";
            std::cout << "Systemd: firmware " << baseline.systemd.firmware << " loader " << baseline.systemd.loader << " kernel " << baseline.systemd.kernel << " initrd " << baseline.systemd.initrd << " userspace " << baseline.systemd.userspace << " failed " << baseline.systemd.failedCount << "\n";
            std::cout << "Thermal max CPU " << baseline.thermal.cpuMaxC << "C throttling " << baseline.thermal.throttling << "\n";
        }
        return 0;
    }
    if(cmd=="performance" && sub=="benchmark"){
        std::string mode="quick";
        for(int i=1;i<argc;i++) if(std::string(argv[i])=="--mode" && i+1<argc) mode=argv[i+1];
        BenchmarkEngine::Mode m = BenchmarkEngine::Mode::QUICK;
        if(mode=="normal") m=BenchmarkEngine::Mode::NORMAL;
        if(mode=="deep") { std::cout << "DEEP benchmark expected load: " << BenchmarkEngine::expectedLoad(m) << " - proceeding (read-only, no config change)\n"; m=BenchmarkEngine::Mode::DEEP; }
        auto res = BenchmarkEngine::run(m, 3);
        if(jsonMode){
            std::cout << "{\n  \"mode\":\"" << mode << "\",\n  \"results\":[\n";
            for(size_t i=0;i<res.size();i++){
                auto &r=res[i];
                std::cout << "    {\"name\":\"" << r.name << "\",\"min\":" << r.min << ",\"max\":" << r.max << ",\"avg\":" << r.avg << ",\"median\":" << r.median << ",\"stddev\":" << r.stddev << ",\"unit\":\"" << r.unit << "\"}";
                if(i+1<res.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  ]\n}\n";
        } else {
            std::cout << "# Benchmark " << mode << " (read-only, non-destructive)\n";
            for(auto &r: res){
                std::cout << std::setw(15) << r.name << " min " << r.min << " max " << r.max << " avg " << r.avg << " median " << r.median << " stddev " << r.stddev << " " << r.unit << " runs " << r.runs << "\n";
            }
        }
        return 0;
    }
    if(cmd=="bottlenecks"){
        if(jsonMode){
            std::cout << "{\n  \"bottlenecks\":[\n";
            for(size_t i=0;i<bottlenecks.size();i++){
                auto &bn=bottlenecks[i];
                std::cout << "    {\"id\":\"" << bn.id << "\",\"category\":\"" << bn.category << "\",\"title\":\"" << json_escape(bn.title) << "\",\"severity\":\"" << bn.severity << "\",\"confidence\":" << bn.confidence << ",\"observed\":\"" << json_escape(bn.observedValue) << "\",\"expected\":\"" << json_escape(bn.expectedValue) << "\"}";
                if(i+1<bottlenecks.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  ]\n}\n";
        } else {
            std::cout << "# Bottlenecks (" << bottlenecks.size() << ")\n";
            for(auto &bn: bottlenecks){
                std::cout << "[" << bn.severity << "] " << bn.id << " " << bn.category << " - " << bn.title << " (conf " << bn.confidence << ")\n";
                std::cout << "  Evidence: ";
                for(auto &e: bn.evidence) std::cout << e << " | ";
                std::cout << "\n";
                std::cout << "  Observed: " << bn.observedValue << " Expected: " << bn.expectedValue << "\n";
                std::cout << "  Impact: " << bn.impact << "\n";
                std::cout << "  Risk: " << bn.risk << "\n\n";
            }
        }
        return 0;
    }
    if(cmd=="recommendations"){
        if(jsonMode){
            std::cout << "{\n  \"recommendations\":[\n";
            for(size_t i=0;i<recs.size();i++){
                auto &r=recs[i];
                std::cout << "    {\"id\":\"" << r.id << "\",\"title\":\"" << json_escape(r.title) << "\",\"risk\":\"" << r.riskLevel << "\",\"reboot\":" << (r.requiresReboot?"true":"false") << ",\"auth\":" << (r.requiresAuth?"true":"false") << ",\"confidence\":" << r.confidence << "}";
                if(i+1<recs.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  ]\n}\n";
        } else {
            std::cout << "# Recommendations (" << recs.size() << ") - INFORMATION ONLY, no apply in P3\n";
            for(auto &r: recs){
                std::cout << "[" << r.riskLevel << "] " << r.id << " " << r.title << " (conf " << r.confidence << ")\n";
                std::cout << "  Problem: " << r.problem << "\n";
                std::cout << "  Benefit: " << r.expectedBenefit << "\n";
                std::cout << "  Why: " << r.why << "\n";
                std::cout << "  Evidence: ";
                for(auto &e: r.evidence) std::cout << e << " | ";
                std::cout << "\n";
                std::cout << "  Affected: " << r.affectedComponent << " Alternative: " << r.alternative << "\n";
                std::cout << "  Rollback: " << r.rollbackConcept << " Reboot:" << r.requiresReboot << " Auth:" << r.requiresAuth << " Approval:" << r.requiresApproval << "\n\n";
            }
        }
        return 0;
    }
    // default analyze
    float totalBaselneMs = baselineMs;
    auto benchQuick = BenchmarkEngine::run(BenchmarkEngine::Mode::QUICK, 3);
    float benchMs=0; for(auto &b: benchQuick) benchMs+=b.avg;

    if(jsonMode){
        std::cout << "{\n";
        std::cout << "  \"baselineId\":\"" << json_escape(baseline.id) << "\",\n";
        std::cout << "  \"baselineMs\":" << totalBaselneMs << ",\n";
        std::cout << "  \"bottlenecks\":" << bottlenecks.size() << ",\n";
        std::cout << "  \"recommendations\":" << recs.size() << ",\n";
        std::cout << "  \"benchmarkQuick\":" << benchQuick.size() << "\n";
        std::cout << "}\n";
    } else {
        std::cout << "# Polaris P3 Analyze - READ-ONLY, no modifications\n";
        std::cout << "Baseline: " << baseline.timestamp << " (" << totalBaselneMs << "ms collect)\n";
        std::cout << "CPU: " << baseline.cpu.model << " " << baseline.cpu.curMhz << "MHz gov " << baseline.cpu.governor << "\n";
        std::cout << "Memory: avail " << baseline.memory.availableKb/1024 << "MB swapUsed " << baseline.memory.swapUsedKb/1024 << "MB pressure " << baseline.memory.pressureSome10 << "\n";
        std::cout << "Boot: firmware " << baseline.systemd.firmware << " loader " << baseline.systemd.loader << " kernel " << baseline.systemd.kernel << " initrd " << baseline.systemd.initrd << " userspace " << baseline.systemd.userspace << " failed " << baseline.systemd.failedCount << "\n";
        std::cout << "Bottlenecks: " << bottlenecks.size() << " (see bottlenecks --human)\n";
        std::cout << "Recommendations: " << recs.size() << " (see recommendations --human) - INFORMATION ONLY\n";
        std::cout << "Benchmark quick: " << benchQuick.size() << " results, avg benchMs " << benchMs << "\n";
        std::cout << "Overhead: baselineMs " << totalBaselneMs << " + bench " << benchMs << " total ~" << (totalBaselneMs+benchMs) << "ms\n";
    }
    return 0;
}
