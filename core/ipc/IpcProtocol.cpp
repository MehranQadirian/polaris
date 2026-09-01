#include "IpcProtocol.h"
#include <sstream>
#include <algorithm>

namespace polaris::ipc {

ValidationResult IpcProtocol::validate(const Request& req){
    ValidationResult r; r.valid=false;

    // NUL checks
    if(containsNul(req.requestId) || containsNul(req.operation)){
        r.reason="NUL byte in requestId or operation"; r.field="requestId/operation"; r.auditOperation="ipc.protocol.error";
        return r;
    }
    for(auto &kv: req.args){
        if(containsNul(kv.first) || containsNul(kv.second)){
            r.reason="NUL byte in args"; r.field=kv.first; r.auditOperation="ipc.protocol.error";
            return r;
        }
    }

    // Protocol version
    if(req.protocolVersion != PROTOCOL_VERSION){
        r.reason="unsupported protocol version: expected 1 observed "+std::to_string(req.protocolVersion);
        r.field="protocolVersion"; r.auditOperation="ipc.protocol.error";
        return r;
    }

    // requestId 1..64
    if(req.requestId.empty() || req.requestId.size()>MAX_FIELD_SIZE){
        r.reason="requestId size invalid: "+std::to_string(req.requestId.size()); r.field="requestId"; r.auditOperation="ipc.protocol.error";
        return r;
    }
    if(containsShellMetachars(req.requestId) || containsTraversal(req.requestId)){
        r.reason="requestId contains shell/traversal"; r.field="requestId"; r.auditOperation="ipc.protocol.error";
        return r;
    }

    // operation 1..64, no NUL, no shell/traversal, allowlist
    if(req.operation.empty() || req.operation.size()>MAX_FIELD_SIZE){
        r.reason="operation size invalid"; r.field="operation"; r.auditOperation="ipc.request.rejected";
        return r;
    }
    if(containsShellMetachars(req.operation) || containsTraversal(req.operation)){
        r.reason="operation contains shell/traversal"; r.field="operation"; r.auditOperation="ipc.request.rejected";
        return r;
    }
    if(allowedOperations().find(req.operation)==allowedOperations().end()){
        // Also catch generic exec
        r.reason="unknown operation: "+req.operation; r.field="operation"; r.auditOperation="ipc.request.rejected";
        return r;
    }

    // args count
    if(req.args.size()>MAX_ARG_COUNT){
        r.reason="too many args: "+std::to_string(req.args.size()); r.field="args"; r.auditOperation="ipc.request.rejected";
        return r;
    }
    // args each
    for(auto &kv: req.args){
        if(kv.first.size()>MAX_FIELD_SIZE){
            r.reason="arg key too large: "+kv.first; r.field=kv.first; r.auditOperation="ipc.request.rejected";
            return r;
        }
        if(kv.second.size()>MAX_ARG_SIZE){
            r.reason="arg value too large for "+kv.first+": "+std::to_string(kv.second.size()); r.field=kv.first; r.auditOperation="ipc.request.rejected";
            return r;
        }
        // No NUL already checked
        // Shell metachars in args values? For safety, reject if args contain shell metachars when key is path-like or generally for all values to avoid exec
        // Spec says validate path safety, no shell metachars, no traversal
        if(containsShellMetachars(kv.second)){
            // For general args, still fail closed if shell metachars present (conservative)
            r.reason="shell metacharacter in arg "+kv.first; r.field=kv.first; r.auditOperation="ipc.request.rejected";
            return r;
        }
        if(containsTraversal(kv.second)){
            r.reason="traversal in arg "+kv.first; r.field=kv.first; r.auditOperation="ipc.request.rejected";
            return r;
        }
        // Password field never allowed
        if(kv.first=="password" || kv.first=="passwd" || kv.first=="secret"){
            r.reason="password field rejected"; r.field=kv.first; r.auditOperation="ipc.request.rejected";
            return r;
        }
        // Also reject if key is password
        std::string low = kv.first;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        if(low.find("password")!=std::string::npos){
            r.reason="password field rejected"; r.field=kv.first; r.auditOperation="ipc.request.rejected";
            return r;
        }
        // UTF-8 check: we just ensure no control chars except maybe \t \n? For now reject control chars <0x20 except \t\n\r
        for(unsigned char c: kv.second){
            if(c < 0x20 && c!='\t' && c!='\n' && c!='\r'){
                r.reason="control char in arg "+kv.first; r.field=kv.first; r.auditOperation="ipc.protocol.error";
                return r;
            }
        }
    }

    r.valid=true; r.reason="request accepted"; r.field=""; r.auditOperation="ipc.request.accepted";
    return r;
}

ValidationResult IpcProtocol::validateRaw(const std::string& raw){
    ValidationResult r;
    if(raw.size()>MAX_REQUEST_SIZE){
        r.valid=false; r.reason="oversized request: "+std::to_string(raw.size())+" > "+std::to_string(MAX_REQUEST_SIZE);
        r.field="request"; r.auditOperation="ipc.protocol.error";
        return r;
    }
    if(raw.find('\0')!=std::string::npos){
        r.valid=false; r.reason="NUL byte in raw frame"; r.field="request"; r.auditOperation="ipc.protocol.error";
        return r;
    }
    // Check for truncated: must end with newline? For our framing we expect newline-terminated JSON, but raw may be without newline if truncated
    // If raw does not contain complete JSON object (missing closing brace), treat as truncated
    // Simple heuristic: count braces, must have both { and }
    if(raw.empty()){
        r.valid=false; r.reason="empty frame"; r.field="request"; r.auditOperation="ipc.protocol.error";
        return r;
    }
    // Check for malformed: must contain protocolVersion, requestId, operation
    // We'll try to parse; if parse throws, it's malformed
    try {
        Request req = parse(raw);
        return validate(req);
    } catch(const std::exception& e){
        r.valid=false; r.reason=std::string("malformed frame: ")+e.what(); r.field="request"; r.auditOperation="ipc.protocol.error";
        return r;
    }
}

std::string IpcProtocol::serialize(const Request& req){
    std::ostringstream oss;
    oss << "{\"protocolVersion\":" << req.protocolVersion << ",\"requestId\":\"" << req.requestId << "\",\"operation\":\"" << req.operation << "\",\"args\":{";
    bool first=true;
    for(auto &kv: req.args){
        if(!first) oss << ",";
        first=false;
        oss << "\"" << kv.first << "\":\"" << kv.second << "\"";
    }
    oss << "}}";
    std::string s = oss.str();
    // Ensure not oversized at serialize time (caller should ensure)
    return s;
}

Request IpcProtocol::parse(const std::string& raw){
    // Minimal JSON parser for our Request format: {"protocolVersion":1,"requestId":"...","operation":"...","args":{"k":"v",...}}
    // We look for required fields, throw on malformed
    Request req;
    std::string s = raw;
    // Remove trailing newline if present
    if(!s.empty() && s.back()=='\n') s.pop_back();
    if(s.empty() || s.front()!='{' || s.back()!='}'){
        throw std::invalid_argument("missing braces");
    }
    // Find protocolVersion
    auto findInt = [&](const std::string& key)->std::optional<int>{
        std::string pat = "\"" + key + "\"";
        auto pos = s.find(pat);
        if(pos==std::string::npos) return std::nullopt;
        auto colon = s.find(':', pos+pat.size());
        if(colon==std::string::npos) throw std::invalid_argument("missing colon for "+key);
        auto comma = s.find(',', colon+1);
        auto brace = s.find('}', colon+1);
        size_t end = std::string::npos;
        if(comma!=std::string::npos && brace!=std::string::npos) end = std::min(comma, brace);
        else if(comma!=std::string::npos) end = comma;
        else if(brace!=std::string::npos) end = brace;
        else throw std::invalid_argument("missing end for "+key);
        std::string numStr = s.substr(colon+1, end-colon-1);
        // Trim
        numStr.erase(0, numStr.find_first_not_of(" \t\n\r"));
        numStr.erase(numStr.find_last_not_of(" \t\n\r")+1);
        try { return std::stoi(numStr); } catch(...){ throw std::invalid_argument("invalid int for "+key); }
    };
    auto findStr = [&](const std::string& key)->std::optional<std::string>{
        std::string pat = "\"" + key + "\"";
        auto pos = s.find(pat);
        if(pos==std::string::npos) return std::nullopt;
        auto colon = s.find(':', pos+pat.size());
        if(colon==std::string::npos) throw std::invalid_argument("missing colon for "+key);
        auto q1 = s.find('"', colon+1);
        if(q1==std::string::npos) throw std::invalid_argument("missing opening quote for "+key);
        auto q2 = s.find('"', q1+1);
        if(q2==std::string::npos) throw std::invalid_argument("missing closing quote for "+key);
        return s.substr(q1+1, q2-q1-1);
    };
    // protocolVersion required
    auto pv = findInt("protocolVersion");
    if(!pv) throw std::invalid_argument("missing protocolVersion");
    req.protocolVersion = *pv;
    auto rid = findStr("requestId");
    if(!rid) throw std::invalid_argument("missing requestId");
    req.requestId = *rid;
    auto op = findStr("operation");
    if(!op) throw std::invalid_argument("missing operation");
    req.operation = *op;
    // args: find "args":{...}
    auto argsPos = s.find("\"args\"");
    if(argsPos!=std::string::npos){
        auto colon = s.find(':', argsPos);
        auto brace1 = s.find('{', colon+1);
        // Find matching braces for args object: simplest find between brace1 and next }
        // Since args is object, find its closing brace before final }
        // We'll search for "args":{ and then find closing } for args
        if(brace1!=std::string::npos){
            // Find closing brace for args: need to find matching }
            int depth=0;
            size_t endPos=std::string::npos;
            for(size_t i=brace1;i<s.size();i++){
                if(s[i]=='{') depth++;
                else if(s[i]=='}'){
                    depth--;
                    if(depth==0){ endPos=i; break; }
                }
            }
            if(endPos!=std::string::npos && endPos>brace1+1){
                std::string argsContent = s.substr(brace1+1, endPos-brace1-1);
                // Parse argsContent as "k":"v",...
                size_t p=0;
                while(p<argsContent.size()){
                    // Find next key
                    auto kq1 = argsContent.find('"', p);
                    if(kq1==std::string::npos) break;
                    auto kq2 = argsContent.find('"', kq1+1);
                    if(kq2==std::string::npos) throw std::invalid_argument("malformed args key");
                    std::string key = argsContent.substr(kq1+1, kq2-kq1-1);
                    auto c = argsContent.find(':', kq2+1);
                    if(c==std::string::npos) throw std::invalid_argument("missing colon in args");
                    auto vq1 = argsContent.find('"', c+1);
                    if(vq1==std::string::npos) throw std::invalid_argument("missing value quote");
                    auto vq2 = argsContent.find('"', vq1+1);
                    if(vq2==std::string::npos) throw std::invalid_argument("missing closing value quote");
                    std::string val = argsContent.substr(vq1+1, vq2-vq1-1);
                    req.args[key]=val;
                    p = vq2+1;
                    // Skip comma
                    auto comma = argsContent.find(',', p);
                    if(comma!=std::string::npos) p = comma+1;
                    else break;
                }
            }
        }
    }
    return req;
}

std::string IpcProtocol::serializeResponse(const Response& resp){
    std::ostringstream oss;
    oss << "{\"protocolVersion\":" << resp.protocolVersion << ",\"requestId\":\"" << resp.requestId << "\",\"status\":\"" << resp.status << "\",\"payload\":{";
    bool first=true;
    for(auto &kv: resp.payload){
        if(!first) oss << ",";
        first=false;
        oss << "\"" << kv.first << "\":\"" << kv.second << "\"";
    }
    oss << "},\"error\":\"" << resp.error << "\"}";
    return oss.str();
}

Response IpcProtocol::parseResponse(const std::string& raw){
    Response r;
    std::string s=raw;
    if(!s.empty() && s.back()=='\n') s.pop_back();
    if(s.empty() || s.front()!='{' || s.back()!='}') throw std::invalid_argument("missing braces response");
    auto findInt = [&](const std::string& key)->std::optional<int>{
        std::string pat = "\"" + key + "\"";
        auto pos = s.find(pat);
        if(pos==std::string::npos) return std::nullopt;
        auto colon = s.find(':', pos+pat.size());
        auto comma = s.find(',', colon+1);
        auto brace = s.find('}', colon+1);
        size_t end = std::string::npos;
        if(comma!=std::string::npos && brace!=std::string::npos) end = std::min(comma, brace);
        else if(comma!=std::string::npos) end = comma;
        else if(brace!=std::string::npos) end = brace;
        std::string numStr = s.substr(colon+1, end-colon-1);
        numStr.erase(0, numStr.find_first_not_of(" \t\n\r"));
        numStr.erase(numStr.find_last_not_of(" \t\n\r")+1);
        try { return std::stoi(numStr); } catch(...){ throw std::invalid_argument("invalid int for "+key); }
    };
    auto findStr = [&](const std::string& key)->std::optional<std::string>{
        std::string pat = "\"" + key + "\"";
        auto pos = s.find(pat);
        if(pos==std::string::npos) return std::nullopt;
        auto colon = s.find(':', pos+pat.size());
        auto q1 = s.find('"', colon+1);
        auto q2 = s.find('"', q1+1);
        if(q1==std::string::npos || q2==std::string::npos) throw std::invalid_argument("missing quote for "+key);
        return s.substr(q1+1, q2-q1-1);
    };
    auto pv = findInt("protocolVersion");
    if(pv) r.protocolVersion=*pv;
    auto rid = findStr("requestId");
    if(rid) r.requestId=*rid;
    auto st = findStr("status");
    if(st) r.status=*st;
    auto err = findStr("error");
    if(err) r.error=*err;
    // payload
    auto payloadPos = s.find("\"payload\"");
    if(payloadPos!=std::string::npos){
        auto colon = s.find(':', payloadPos);
        auto brace1 = s.find('{', colon+1);
        if(brace1!=std::string::npos){
            int depth=0;
            size_t endPos=std::string::npos;
            for(size_t i=brace1;i<s.size();i++){
                if(s[i]=='{') depth++;
                else if(s[i]=='}'){
                    depth--;
                    if(depth==0){ endPos=i; break; }
                }
            }
            if(endPos!=std::string::npos && endPos>brace1+1){
                std::string content = s.substr(brace1+1, endPos-brace1-1);
                size_t p=0;
                while(p<content.size()){
                    auto kq1 = content.find('"', p);
                    if(kq1==std::string::npos) break;
                    auto kq2 = content.find('"', kq1+1);
                    if(kq2==std::string::npos) break;
                    std::string key = content.substr(kq1+1, kq2-kq1-1);
                    auto c = content.find(':', kq2+1);
                    auto vq1 = content.find('"', c+1);
                    auto vq2 = content.find('"', vq1+1);
                    if(vq1==std::string::npos || vq2==std::string::npos) break;
                    std::string val = content.substr(vq1+1, vq2-vq1-1);
                    r.payload[key]=val;
                    p=vq2+1;
                    auto comma = content.find(',', p);
                    if(comma!=std::string::npos) p=comma+1;
                    else break;
                }
            }
        }
    }
    return r;
}

} // namespace polaris::ipc
