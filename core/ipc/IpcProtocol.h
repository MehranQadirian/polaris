#pragma once
#include <string>
#include <map>
#include <set>
#include <vector>
#include <optional>
#include <stdexcept>

namespace polaris::ipc {

struct Request {
    int protocolVersion = 1;
    std::string requestId; // 1..64
    std::string operation; // 1..64, allowlist
    std::map<std::string, std::string> args; // ≤16, each ≤4096
};

struct Response {
    int protocolVersion = 1;
    std::string requestId;
    std::string status; // "ok" or "error"
    std::map<std::string, std::string> payload;
    std::string error;
};

struct ValidationResult {
    bool valid = false;
    std::string reason;
    std::string field;
    std::string auditOperation; // e.g., "ipc.request.accepted", "ipc.protocol.error"
};

class IpcProtocol {
public:
    static constexpr int PROTOCOL_VERSION = 1;
    static constexpr size_t MAX_REQUEST_SIZE = 64 * 1024;
    static constexpr size_t MAX_RESPONSE_SIZE = 64 * 1024;
    static constexpr size_t MAX_ARG_COUNT = 16;
    static constexpr size_t MAX_ARG_SIZE = 4096;
    static constexpr size_t MAX_FIELD_SIZE = 256;
    static constexpr int TIMEOUT_MS = 5000;

    static const std::set<std::string>& allowedOperations(){
        static const std::set<std::string> ops = {"ping", "info"};
        return ops;
    }

    // Pure validation of structured Request
    static ValidationResult validate(const Request& req);

    // Validate raw JSON string (size, framing, parse, then validate structured)
    static ValidationResult validateRaw(const std::string& raw);

    // Serialize / parse (deterministic, minimal JSON)
    static std::string serialize(const Request& req);
    static Request parse(const std::string& raw); // throws on malformed
    static std::string serializeResponse(const Response& resp);
    static Response parseResponse(const std::string& raw);

    // Helpers for framing
    static bool containsNul(const std::string& s){ return s.find('\0')!=std::string::npos; }
    static bool containsTraversal(const std::string& s){ return s.find("..")!=std::string::npos; }
    static bool containsShellMetachars(const std::string& s){
        return s.find(';')!=std::string::npos || s.find('|')!=std::string::npos || s.find('&')!=std::string::npos
            || s.find('`')!=std::string::npos || s.find('$')!=std::string::npos;
    }
};

} // namespace polaris::ipc
