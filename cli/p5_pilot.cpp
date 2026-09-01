#include "../core/safety/transaction/StateMachine.h"
#include "../core/safety/transaction/Transaction.h"
#include "../core/safety/FileSafety.h"
#include "../core/safety/backup/BackupEngine.h"
#include "../core/safety/audit/AuditLog.h"
#include "../core/providers/real/RealGpuProvider.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

using namespace polaris::safety;

std::string nowISO(){
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    char buf[64]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S%z", localtime(&tt));
    return buf;
}
std::string sha256File(const std::string& path){
    return BackupEngine::sha256File(path);
}

int main(int argc, char** argv){
    std::string home = getenv("HOME") ? getenv("HOME") : "/home/mehrangh";
    std::string target = home + "/.config/autostart/nvidia-settings-user.desktop";
    std::string mode = argc>1 ? argv[1] : "preview"; // preview, apply, verify, rollback-test

    // 1. PRECONDITION CHECK
    std::cout << "# P5 Pilot Precondition Check - " << target << "\n";
    bool fail=false;
    auto check = [&](bool cond, const std::string& msg){
        std::cout << (cond ? "[OK] " : "[FAIL] ") << msg << "\n";
        if(!cond) fail=true;
    };

    // Fedora compatible
    std::ifstream os("/etc/os-release");
    std::string osContent((std::istreambuf_iterator<char>(os)), {});
    check(osContent.find("fedora")!=std::string::npos, "Fedora environment compatible (os-release contains fedora)");

    struct stat st;
    check(stat(target.c_str(), &st)==0, "file exists");
    if(stat(target.c_str(), &st)!=0) { std::cerr << "STOP: file missing, no modification\n"; return 1; }
    check(S_ISREG(st.st_mode), "file is regular file");
    check(!FileSafety::isSymlink(target), "file is not symlink");
    std::string canon;
    try { canon = FileSafety::canonical(target); check(canon==target, "canonical path exactly matches target"); }
    catch(const std::exception& e){ std::cout << "[FAIL] canonical: " << e.what() << "\n"; fail=true; }

    check(st.st_uid == getuid(), "file owned by current user ("+std::to_string(st.st_uid)+" vs "+std::to_string(getuid())+")");
    check(st.st_size < 4096 && st.st_size>0, "file size within safe limits ("+std::to_string(st.st_size)+" bytes)");
    // contains expected NVIDIA entry
    {
        std::ifstream f(target);
        std::string content((std::istreambuf_iterator<char>(f)), {});
        check(content.find("nvidia-settings")!=std::string::npos, "file contains expected NVIDIA settings entry");
        check(content.find("Hidden=true")!=std::string::npos || content.find("Hidden=false")!=std::string::npos || content.find("Hidden")!=std::string::npos, "file contains Hidden entry or will add");
    }
    std::string beforeHash = sha256File(target);
    check(!beforeHash.empty(), "current file hash recorded: "+beforeHash.substr(0,16)+"...");

    if(fail){
        std::cerr << "STOP: Precondition failed - no modification\n";
        return 1;
    }
    std::cout << "All preconditions PASS - proceed to transaction\n\n";

    if(mode=="preview"){
        // 2. CREATE TRANSACTION
        Transaction tx;
        tx.id = "TX-P5-20260831-001";
        tx.operationId = "nvidia-autostart-hidden-true";
        tx.target = target;
        tx.description = "Disable user-level nvidia-settings autostart via Hidden=true (P5 pilot, R1)";
        tx.riskLevel = "R1";
        tx.expectedBenefit = "Avoid 2.56s login overhead and driver-not-loaded errors";
        tx.requiredPrivileges = "none (user-owned file, no Polkit)";
        tx.state = TxState::PREVIEWED;
        tx.approvalState = "PENDING";
        tx.authorizationState = "NONE_REQUIRED";
        tx.rebootRequired = false;
        tx.timestamp = nowISO();

        std::ifstream f(target);
        std::string before((std::istreambuf_iterator<char>(f)), {});
        tx.beforeState = before;
        std::string beforeHash2 = sha256File(target);

        // Proposed state: ensure Hidden=true
        std::string after;
        if(before.find("Hidden=true")!=std::string::npos){
            after = before; // already correct
        } else if(before.find("Hidden=")!=std::string::npos){
            // replace Hidden=false -> Hidden=true
            after = before;
            size_t pos = after.find("Hidden=");
            size_t end = after.find("\n", pos);
            after.replace(pos, end-pos, "Hidden=true");
        } else {
            // append Hidden=true
            after = before;
            if(after.back()!='\n') after += "\n";
            after += "Hidden=true\n";
        }
        tx.afterState = after;

        ChangePreview cp;
        cp.target = target;
        cp.beforeState = before;
        cp.afterState = after;
        // diff
        if(before==after) cp.diff = "(no diff - already Hidden=true, idempotent)";
        else cp.diff = "- Hidden=false (or missing)\n+ Hidden=true\n";
        cp.method = "atomic write via FileSafety::atomicWrite (user file, no sudo, no Polkit)";
        cp.privilege = "none (user-owned)";
        cp.risk = "R1 low, reversible";
        cp.benefit = "Avoid 2.56s login overhead and journal ERROR driver not loaded";
        cp.rollback = "Restore from backup " + std::string(BackupEngine::backupRoot()) + "/" + tx.id + "/nvidia-settings-user.desktop.bak";
        cp.rebootRequired = false;
        tx.previews.push_back(cp);

        // Save transaction preview
        std::filesystem::create_directories(std::string(getenv("HOME"))+"/.local/state/polaris/transactions");
        std::string txPath = std::string(getenv("HOME"))+"/.local/state/polaris/transactions/"+tx.id+".json";
        {
            std::ofstream out(txPath);
            out << "{\n  \"id\":\"" << tx.id << "\",\n";
            out << "  \"operationId\":\"" << tx.operationId << "\",\n";
            out << "  \"target\":\"" << tx.target << "\",\n";
            out << "  \"risk\":\"" << tx.riskLevel << "\",\n";
            out << "  \"state\":\"" << toString(tx.state) << "\",\n";
            out << "  \"beforeHash\":\"" << beforeHash2 << "\",\n";
            out << "  \"approvalState\":\"" << tx.approvalState << "\"\n}\n";
        }

        // Audit previewed
        AuditEvent ev{nowISO(), tx.id, "transaction.previewed", "mehrangh", "PENDING", "NONE_REQUIRED", "", cp.diff, "", "", "", "", ""};
        AuditLog::append(ev);

        std::cout << "===== PREVIEW =====\n";
        std::cout << "TARGET: " << target << "\n\n";
        std::cout << "CURRENT STATE (" << before.size() << " bytes, sha256 " << beforeHash2.substr(0,16) << "...):\n";
        std::cout << before << "\n";
        std::cout << "PROPOSED STATE:\n";
        std::cout << after << "\n";
        std::cout << "DIFF: " << cp.diff << "\n";
        std::cout << "WHY: NVIDIA settings autostart launches while NVIDIA driver currently unavailable (GM108 Maxwell, open driver 610 requires GSP, probe fails, journal NVRM 26-99/boot).\n";
        std::cout << "EVIDENCE: /sys/bus/pci driver missing claimed false, nvidia-smi stat exists but probe fails, journal NVRM not supported, prior manual measured 2.56s CPU/login overhead, glRenderer Intel only.\n";
        std::cout << "EXPECTED BENEFIT: Avoid unnecessary startup/login work and driver-not-loaded errors (2.56s).\n";
        std::cout << "RISK: R1 low and reversible (user file, no system files).\n";
        std::cout << "ROLLBACK: Restore exact original file from transaction backup " << cp.rollback << "\n";
        std::cout << "REBOOT: false (user session, next login)\n";
        std::cout << "AUTHENTICATION: not required (current user owns file, Polkit not needed)\n";
        std::cout << "STATE: PREVIEWED -> APPROVAL_REQUIRED\n";
        std::cout << "TRANSACTION ID: " << tx.id << "\n";
        std::cout << "BEFORE HASH: " << beforeHash2 << "\n";
        std::cout << "STOP: Awaiting explicit user approval tied to transaction ID, target, expected change, and before hash.\n";
        std::cout << "To approve: /tmp/polaris_build/polaris_p5 approve " << tx.id << "\n";
        return 0;
    }

    if(mode=="approve"){
        std::string txId = argc>2 ? argv[2] : "TX-P5-20260831-001";
        std::string txPath = home+"/.local/state/polaris/transactions/"+txId+".json";
        std::ifstream f(txPath);
        if(!f){ std::cerr << "Transaction not found: " << txPath << "\n"; return 1; }
        // Verify file hash still matches beforeHash (no concurrent modification)
        std::string currentHash = sha256File(target);
        // Read stored beforeHash
        std::string content((std::istreambuf_iterator<char>(f)), {});
        auto pos = content.find("\"beforeHash\":\"");
        std::string storedHash;
        if(pos!=std::string::npos){
            size_t s=pos+14; size_t e=content.find('"',s);
            storedHash=content.substr(s,e-s);
        }
        if(!storedHash.empty() && currentHash!=storedHash){
            std::cerr << "FAIL: File changed after preview (beforeHash " << storedHash.substr(0,16) << " vs current " << currentHash.substr(0,16) << ") - invalidate approval, require new preview\n";
            AuditEvent ev{nowISO(), txId, "approval.rejected", "mehrangh", "REJECTED", "NONE", "", "hash mismatch", "", "", "", "", ""};
            AuditLog::append(ev);
            return 1;
        }
        std::cout << "[OK] Approval tied to transaction " << txId << " target " << target << " beforeHash " << currentHash.substr(0,16) << "... - APPROVED\n";
        AuditEvent ev{nowISO(), txId, "transaction.approved", "mehrangh", "APPROVED", "NONE", "", "", "", "", "", "", ""};
        AuditLog::append(ev);
        // Update transaction state to APPROVED
        std::string approvedPath = txPath;
        std::ifstream in(approvedPath);
        std::string json((std::istreambuf_iterator<char>(in)), {});
        // simple replace state
        size_t p = json.find("\"state\":\"PREVIEWED\"");
        if(p!=std::string::npos) json.replace(p, 19, "\"state\":\"APPROVED\"");
        size_t q = json.find("\"approvalState\":\"PENDING\"");
        if(q!=std::string::npos) json.replace(q, 24, "\"approvalState\":\"APPROVED\"");
        std::ofstream out(approvedPath);
        out << json;
        std::cout << "State: APPROVED - ready for backup/apply\n";
        return 0;
    }

    if(mode=="apply"){
        std::string txId = "TX-P5-20260831-001";
        std::string txPath = home+"/.local/state/polaris/transactions/"+txId+".json";
        // Check approved
        std::ifstream f(txPath);
        if(!f){ std::cerr << "No transaction, run preview first\n"; return 1; }
        std::string json((std::istreambuf_iterator<char>(f)), {});
        if(json.find("\"state\":\"APPROVED\"")==std::string::npos){
            std::cerr << "Transaction not approved - run approve first\n";
            return 1;
        }

        // 5. BACKUP
        std::cout << "# Backup...\n";
        Backup backup;
        try {
            backup = BackupEngine::create(txId, target);
            std::cout << "[OK] Backup created: " << backup.backupPath << " sha256 " << backup.sha256.substr(0,16) << " size " << backup.size << " perms " << backup.permissions << "\n";
            AuditEvent ev{nowISO(), txId, "backup.created", "mehrangh", "APPROVED", "NONE", backup.backupPath, "", "", "", "", "", ""};
            AuditLog::append(ev);
        } catch(const std::exception& e){
            std::cerr << "Backup failed: " << e.what() << " - STOP, no modification\n";
            AuditEvent ev{nowISO(), txId, "backup.failed", "mehrangh", "APPROVED", "NONE", "", "", "", "", e.what(), "", ""};
            AuditLog::append(ev);
            return 1;
        }

        // Verify backup integrity
        std::string backupHash = BackupEngine::sha256File(backup.backupPath);
        if(backupHash!=backup.sha256){
            std::cerr << "Backup hash mismatch - STOP\n";
            return 1;
        }

        // 6. APPLY Hidden=true
        std::cout << "# Apply Hidden=true via atomic write (no sudo, no Polkit)...\n";
        std::ifstream in(target);
        std::string before((std::istreambuf_iterator<char>(in)), {});
        std::string after;
        if(before.find("Hidden=true")!=std::string::npos) after=before;
        else if(before.find("Hidden=")!=std::string::npos){
            after=before;
            size_t pos=after.find("Hidden=");
            size_t end=after.find("\n",pos);
            after.replace(pos, end-pos, "Hidden=true");
        } else {
            after=before;
            if(after.back()!='\n') after+="\n";
            after+="Hidden=true\n";
        }

        try {
            FileSafety::atomicWrite(target, after);
            std::cout << "[OK] Applied Hidden=true via atomic write\n";
            AuditEvent ev{nowISO(), txId, "transaction.applied", "mehrangh", "APPROVED", "NONE", backup.backupPath, "Hidden=true", "", "", "", "", ""};
            AuditLog::append(ev);
        } catch(const std::exception& e){
            std::cerr << "Apply failed: " << e.what() << "\n";
            return 1;
        }

        // 7. VERIFICATION
        std::cout << "# Verification...\n";
        std::ifstream vf(target);
        std::string now((std::istreambuf_iterator<char>(vf)), {});
        std::string afterHash = BackupEngine::sha256File(target);
        struct stat st2; stat(target.c_str(), &st2);
        bool ok = (now.find("Hidden=true")!=std::string::npos
                && S_ISREG(st2.st_mode)
                && st2.st_uid==getuid()
                && (st2.st_mode & 0777)==0644
                && afterHash==BackupEngine::sha256File(target));
        if(!ok){
            std::cerr << "Verification failed - rollback immediately\n";
            BackupEngine::restore(backup);
            return 1;
        }
        std::cout << "[OK] Verification passed: Hidden=true present, regular file, owned, perms 0644, sha256 " << afterHash.substr(0,16) << "\n";
        AuditEvent vev{nowISO(), txId, "verification.passed", "mehrangh", "APPROVED", "NONE", backup.backupPath, afterHash, "", "", "", "", ""};
        AuditLog::append(vev);

        std::cout << "P5 Apply complete - change active, not rolled back per pilot (keep active)\n";
        return 0;
    }

    std::cout << "Usage: polaris_p5 <preview|approve <id>|apply>\n";
    return 0;
}
