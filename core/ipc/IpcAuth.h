#pragma once
#include <string>
#include <optional>
#include <map>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace polaris::ipc {

struct PeerCred {
    pid_t pid = 0;
    uid_t uid = 0;
    gid_t gid = 0;
};

class IpcAuth {
public:
    // Get peer credentials via SO_PEERCRED (Linux). Returns nullopt on failure / unavailable.
    static std::optional<PeerCred> getPeerCred(int fd);

    // Check if peer is authorized (same UID). Other checks could be added (PID, GID).
    static bool isAuthorized(const PeerCred& cred, uid_t expectedUid);

    // Convenience: get expected UID (current user)
    static uid_t currentUid(){ return getuid(); }

    // Validate raw credentials map from Request (should not contain uid field) - helper to ensure no client-supplied UID trusted
    static bool containsSpoofedCred(const std::map<std::string,std::string>& args);
};

} // namespace polaris::ipc
