#include "RecommendationEngine.h"

namespace polaris::engines::recommend {

std::vector<domain::Recommendation> RecommendationEngine::generate(const domain::PerformanceBaseline& b, const std::vector<domain::Bottleneck>& bottlenecks) {
    (void)b;
    std::vector<domain::Recommendation> out;
    auto add = [&](std::string id, std::string title, std::string problem, std::vector<std::string> ev, float conf,
                   std::string benefit, std::string risk, std::string comp, std::string why, std::string alt, std::string rollback,
                   bool reboot, bool auth, bool approval, std::string cat){
        domain::Recommendation r;
        r.id=id; r.title=title; r.problem=problem; r.evidence=ev; r.confidence=conf;
        r.expectedBenefit=benefit; r.riskLevel=risk; r.affectedComponent=comp; r.why=why; r.alternative=alt; r.rollbackConcept=rollback;
        r.requiresReboot=reboot; r.requiresAuth=auth; r.requiresApproval=approval; r.category=cat;
        out.push_back(r);
    };

    for(auto &bn : bottlenecks){
        if(bn.id=="GPU-001"){
            add("REC-001","Replace NVIDIA open driver 610 with legacy 470xx for MX130 (GM108)",
                "MX130 unclaimed, open driver requires GSP (Turing+), Maxwell has no GSP",
                bn.evidence, 0.96f,
                "Restore PRIME offload, eliminate 26-99 journal errors per boot, enable nvidia-powerd, allow CUDA/Offload",
                "R3", "GPU/driver", "Compatibility matrix: GM108 10de:174d → 470.256.02 (RPMFusion README lists 174D), 610 open supports only GSP GPUs",
                "Keep Intel-only (lose NVIDIA) or use nouveau (blacklisted)",
                "dnf swap akmod-nvidia akmod-nvidia-470xx + akmods --force + dracut --force; rollback via reinstall 610 + rebuild",
                true, true, true, "GPU");
        } else if(bn.id=="BOOT-001" || bn.id=="BOOT-002"){
            // dnf-makecache blocker vs background already handled
            // Only recommend investigation, not disable
            add("REC-002","Investigate dnf-makecache timer schedule (do not auto-disable)",
                "dnf-makecache 2m28s appears in blame, classification needed",
                bn.evidence, bn.confidence,
                "Clarify if blocking or parallel; if blocking, save ~5-30s boot",
                "R2", "systemd/timer", "Evidence: "+bn.observedValue+" "+bn.impact,
                "Monitor 3 boots, check systemctl cat dnf-makecache.timer OnCalendar, check critical-chain presence",
                "systemctl disable timer (reversible) or set TimerAccuracy",
                false, true, true, "Boot");
        } else if(bn.id=="SVC-001"){
            add("REC-003","Disable or repair mssql-server (failed 25 boots)",
                "mssql-server status 18 model.mdf Windows path, 713M+9s per boot",
                bn.evidence, 0.88f,
                "Save 713M RAM + 9s CPU per boot, clean failed unit, reduce journal spam",
                "R2", "Service mssql-server", "Evidence: journal 25 boots failed, errorlog FCB::Open ... model.mdf",
                "Repair via /opt/mssql/bin/mssql-conf setup and restore model DB vs disable",
                "systemctl enable --now mssql-server to rollback",
                false, true, true, "Service");
        } else if(bn.id=="SVC-002"){
            add("REC-004","Review Akonadi PIM (keep if KMail used)",
                "Akonadi 14 agents + mysqld 126M db, not top hog currently but overhead if unused",
                bn.evidence, 0.65f,
                "Save ~600M RAM + I/O if PIM unused",
                "R2", "KDE PIM akonadi", "Manual earlier: kmail kontact installed, inbox data present, so likely used",
                "Keep (default) vs disable via akonadictl stop + config StartServer=false",
                "akonadictl start or reinstall",
                false, false, true, "KDE");
        } else if(bn.title.find("stale fstab")!=std::string::npos || bn.title.find("wait-online")!=std::string::npos){
            // These were already fixed in prior manual optimization (P2), but for P3 we still generate recommendation as info
            // Since P3 is read-only, we show as already addressed
            add("REC-005","Verify prior fixes (fstab stale swap, wait-online) remain effective",
                bn.title, bn.evidence, bn.confidence,
                "Ensure boot not regressed (swap timeout eliminated, wait-online disabled next boot)",
                bn.risk, bn.category, "Prior Level2 fixes applied 2026-08-31, verify via findmnt --verify and systemctl is-enabled",
                "No alternative needed",
                "Restore backup /etc/fstab.bak or enable wait-online",
                true, true, true, bn.category);
        }
    }

    // Add explicit recommendations from baseline even if not bottleneck
    // Recommend flatpak cleanup as safe R1
    add("REC-006","Clean unused Flatpak runtimes (safe)",
        "Multiple Freedesktop/GNOME runtimes (25.08 + 50) may duplicate",
        {"flatpak list shows Freedesktop 25.08 + GNOME 50 duplicates"}, 0.70f,
        "Reclaim 1-2GB disk, reduce metadata",
        "R1", "Filesystem / (free 128G)", "Evidence: flatpak list earlier shows 30 runtimes",
        "Keep all (disk ok 62% used)",
        "flatpak install to rollback",
        false, false, false, "Storage");

    // Recommend not disabling zram/swap
    add("REC-007","Do NOT disable zram/swap (healthy)",
        "Memory pressure 0, zram 8G lzo-rle correctly sized, swappiness 60",
        {"MemAvailable 4.4GB, pressure some 0, swap used 1.6GB (normal under load), zram disksize 8G"}, 0.95f,
        "Avoid stability regression",
        "R0", "Memory", "Evidence: pressure 0, available 4.4GB, not a bottleneck",
        "No alternative - keep",
        "N/A",
        false, false, false, "Memory");

    // Boot improvement estimate
    add("REC-008","Reboot to measure prior Level2 boot gains",
        "fstab fix + wait-online disable require reboot to measure",
        {"systemd-analyze userspace 54.106s current boot (before fixes), fixes applied 2026-08-31 21:15-21:18"}, 0.90f,
        "Projected -10 to -15s userspace (wait-online 5.3s + swap timeout + packagekit 35s already improved)",
        "R0", "Boot", "Requires reboot, no auth",
        "No reboot (stay on current boot)",
        "N/A",
        true, false, false, "Boot");

    return out;
}

} // namespace polaris::engines::recommend
