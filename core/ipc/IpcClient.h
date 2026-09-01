#pragma once
#include "IpcProtocol.h"
#include <string>
#include <optional>

namespace polaris::ipc {

class IpcClient {
public:
    explicit IpcClient(const std::string& socketPath = defaultSocketPath());
    static std::string defaultSocketPath(){ return "/run/polaris/helper.sock"; }
    static std::string testSocketPath(){ return "/tmp/polaris-test-root/p14/helper.sock"; }
    static std::string testSocketPath(const std::string& dir);

    // Send request and receive response with timeout, returns nullopt on failure
    std::optional<Response> send(const Request& req, int timeoutMs = IpcProtocol::TIMEOUT_MS);

    // Raw send for testing malformed/oversized
    std::optional<std::string> sendRaw(const std::string& raw, int timeoutMs = IpcProtocol::TIMEOUT_MS);

private:
    std::string socketPath_;
};

} // namespace polaris::ipc
