#pragma once
#include "IpcProtocol.h"
#include "IpcAuth.h"
#include "../safety/audit/AuditLog.h"
#include <string>
#include <optional>
#include <filesystem>
#include <sys/socket.h>
#include <sys/un.h>

namespace polaris::ipc {

class IpcServer {
public:
    explicit IpcServer(const std::string& socketPath = defaultSocketPath());
    ~IpcServer();

    static std::string defaultSocketPath(){
        return "/run/polaris/helper.sock";
    }
    static std::string testSocketPath(){
        return "/tmp/polaris-test-root/p14/helper.sock";
    }
    static std::string testSocketPath(const std::string& dir){
        if(dir.empty()) return testSocketPath();
        if(dir.back()=='/') return dir + "helper.sock";
        return dir + "/helper.sock";
    }

    // Socket security checks and setup
    bool start(); // bind + listen, returns false on failure (audit)
    void stop(); // close + unlink + audit

    bool isRunning() const { return listenFd_ >=0; }
    std::string socketPath() const { return socketPath_; }
    int listenFd() const { return listenFd_; }

    // Handle one request synchronously (for testing without full event loop)
    // Takes raw request string and peer cred, returns response (or error response if validation fails)
    Response handleRequest(const std::string& raw, const std::optional<PeerCred>& peerCred);

    // Accept one connection with timeout, perform SO_PEERCRED auth, read request, handle, send response
    // Returns true if handled, false on timeout/error (still audit)
    bool handleNextConnection(int timeoutMs = IpcProtocol::TIMEOUT_MS);

    // Socket security validation (call before bind)
    static ValidationResult validateSocketPath(const std::string& path);
    static ValidationResult checkParentSecurity(const std::string& path);
    static bool isStaleSocket(const std::string& path);

private:
    std::string socketPath_;
    int listenFd_ = -1;
    bool validateAndPrepare();
    Response makeErrorResponse(const Request& req, const std::string& error);
    Response makeErrorResponseForRaw(const std::string& raw, const std::string& error);
    void audit(const std::string& op, const std::string& detail, const std::string& requestId = "");
    static std::string nowISO();
};

} // namespace polaris::ipc
