#include "../../core/ipc/IpcProtocol.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace polaris::ipc;

void test_protocol_version_accepted(){
    Request req{1, "REQ-001", "ping", {}};
    auto vr = IpcProtocol::validate(req);
    assert(vr.valid);
    assert(vr.auditOperation=="ipc.request.accepted");
    std::cout << "protocol version accepted PASS\n";
}
void test_unsupported_protocol_rejected(){
    Request req{2, "REQ-002", "ping", {}};
    auto vr = IpcProtocol::validate(req);
    assert(!vr.valid);
    assert(vr.auditOperation=="ipc.protocol.error");
    std::cout << "unsupported protocol rejected PASS\n";
    // Also via raw
    std::string raw = "{\"protocolVersion\":2,\"requestId\":\"REQ-002\",\"operation\":\"ping\",\"args\":{}}";
    auto vrr = IpcProtocol::validateRaw(raw);
    assert(!vrr.valid);
    std::cout << "unsupported protocol via raw rejected PASS\n";
}
void test_ping_success(){
    Request req{1, "REQ-PING", "ping", {}};
    std::string raw = IpcProtocol::serialize(req);
    auto vr = IpcProtocol::validateRaw(raw);
    assert(vr.valid);
    Request parsed = IpcProtocol::parse(raw);
    assert(parsed.operation=="ping");
    std::cout << "ping success PASS\n";
}
void test_malformed_frame_rejected(){
    // Missing closing brace
    std::string raw = "{\"protocolVersion\":1,\"requestId\":\"REQ-003\",\"operation\":\"ping\",\"args\":{";
    auto vr = IpcProtocol::validateRaw(raw);
    assert(!vr.valid);
    assert(vr.auditOperation=="ipc.protocol.error");
    std::cout << "malformed frame rejected PASS\n";
    // Missing required field
    std::string raw2 = "{\"protocolVersion\":1,\"requestId\":\"REQ-003\",\"args\":{}}";
    auto vr2 = IpcProtocol::validateRaw(raw2);
    assert(!vr2.valid);
    std::cout << "malformed missing operation rejected PASS\n";
}
void test_oversized_request_rejected(){
    std::string large(IpcProtocol::MAX_REQUEST_SIZE+1, 'A');
    // Create raw that is oversized via args
    std::string raw = "{\"protocolVersion\":1,\"requestId\":\"REQ-OVER\",\"operation\":\"ping\",\"args\":{\"big\":\""+large+"\"}}";
    assert(raw.size() > IpcProtocol::MAX_REQUEST_SIZE);
    auto vr = IpcProtocol::validateRaw(raw);
    assert(!vr.valid);
    assert(vr.reason.find("oversized")!=std::string::npos);
    std::cout << "oversized request rejected PASS\n";
}
void test_truncated_request_rejected(){
    // Truncated: incomplete JSON (missing operation, no closing brace)
    std::string raw = "{\"protocolVersion\":1,\"requestId\":\"REQ-TRUNC\"";
    auto vr = IpcProtocol::validateRaw(raw);
    assert(!vr.valid);
    std::cout << "truncated request rejected PASS\n";
}
void test_unknown_operation_rejected(){
    Request req{1, "REQ-004", "unknownOp", {}};
    auto vr = IpcProtocol::validate(req);
    assert(!vr.valid);
    assert(vr.auditOperation=="ipc.request.rejected");
    std::cout << "unknown operation rejected PASS\n";
}
void test_arbitrary_command_rejected(){
    // Attempt to use generic exec
    Request req{1, "REQ-005", "exec", {{"cmd","ls -la"}}};
    auto vr = IpcProtocol::validate(req);
    assert(!vr.valid);
    std::cout << "arbitrary command (exec) rejected PASS\n";
    Request req2{1, "REQ-005b", "execute", {{"command","rm -rf /"}}};
    assert(!IpcProtocol::validate(req2).valid);
    std::cout << "arbitrary command (execute) rejected PASS\n";
}
void test_shell_command_rejected(){
    Request req{1, "REQ-006", "ping", {{"path","/tmp/test; rm -rf /"}}};
    auto vr = IpcProtocol::validate(req);
    assert(!vr.valid);
    assert(vr.field.find("path")!=std::string::npos || vr.reason.find("shell")!=std::string::npos);
    std::cout << "shell command rejected PASS\n";
    Request req2{1, "REQ-006b", "ping", {{"arg","a|b"}}};
    assert(!IpcProtocol::validate(req2).valid);
    std::cout << "shell pipe rejected PASS\n";
}
void test_traversal_rejected(){
    Request req{1, "REQ-007", "ping", {{"path","../../etc/passwd"}}};
    auto vr = IpcProtocol::validate(req);
    assert(!vr.valid);
    std::cout << "traversal rejected PASS\n";
    Request req2{1, "REQ-007b", "ping", {{"file",".."}}};
    assert(!IpcProtocol::validate(req2).valid);
    std::cout << "traversal .. rejected PASS\n";
}
void test_nul_rejected(){
    std::string nulStr = "ping";
    nulStr.push_back('\0');
    nulStr += "hidden";
    Request req{1, "REQ-NUL", nulStr, {}};
    auto vr = IpcProtocol::validate(req);
    assert(!vr.valid);
    std::cout << "NUL in operation rejected PASS\n";
    std::string raw = "{\"protocolVersion\":1,\"requestId\":\"REQ-NUL\",\"operation\":\"ping\",\"args\":{\"k\":\"a";
    raw.push_back('\0');
    raw += "b\"}}";
    auto vrr = IpcProtocol::validateRaw(raw);
    assert(!vrr.valid);
    std::cout << "NUL in raw rejected PASS\n";
}
void test_oversized_argument_rejected(){
    std::string big(IpcProtocol::MAX_ARG_SIZE+1, 'X');
    Request req{1, "REQ-008", "ping", {{"big", big}}};
    auto vr = IpcProtocol::validate(req);
    assert(!vr.valid);
    std::cout << "oversized argument rejected PASS\n";
}

int main(){
    test_protocol_version_accepted();
    test_unsupported_protocol_rejected();
    test_ping_success();
    test_malformed_frame_rejected();
    test_oversized_request_rejected();
    test_truncated_request_rejected();
    test_unknown_operation_rejected();
    test_arbitrary_command_rejected();
    test_shell_command_rejected();
    test_traversal_rejected();
    test_nul_rejected();
    test_oversized_argument_rejected();
    std::cout << "All P14 IPC protocol tests PASS (12 cases)\n";
    return 0;
}
