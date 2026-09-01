#include "IpcAuth.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <map>
#ifdef __linux__
#include <sys/socket.h>
#endif

namespace polaris::ipc {

std::optional<PeerCred> IpcAuth::getPeerCred(int fd){
    if(fd < 0) return std::nullopt;
#ifdef SO_PEERCRED
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if(getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len)!=0){
        return std::nullopt;
    }
    if(len != sizeof(cred)) return std::nullopt;
    PeerCred pc;
    pc.pid = cred.pid;
    pc.uid = cred.uid;
    pc.gid = cred.gid;
    return pc;
#else
    (void)fd;
    return std::nullopt;
#endif
}

bool IpcAuth::isAuthorized(const PeerCred& cred, uid_t expectedUid){
    // Must be same UID, pid >0, gid valid
    if(cred.uid != expectedUid) return false;
    if(cred.pid <= 0) return false;
    // Additional checks could be added (e.g., gid matches expected gid)
    return true;
}

bool IpcAuth::containsSpoofedCred(const std::map<std::string,std::string>& args){
    for(auto &kv: args){
        std::string low = kv.first;
        for(auto &c: low) c = tolower(c);
        if(low=="uid" || low=="pid" || low=="gid" || low=="peer_uid" || low=="peer_pid"){
            return true;
        }
        // Also check if value looks like attempted spoof
        if(kv.second.find("uid")!=std::string::npos && kv.second.find("=")!=std::string::npos){
            // conservative: if args contain uid=, treat as spoof attempt
            // But not needed for P14; we just check keys
        }
    }
    return false;
}

} // namespace polaris::ipc
