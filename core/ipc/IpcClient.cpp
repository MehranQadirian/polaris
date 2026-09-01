#include "IpcClient.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <fcntl.h>

namespace polaris::ipc {

IpcClient::IpcClient(const std::string& socketPath) : socketPath_(socketPath) {}

std::string IpcClient::testSocketPath(const std::string& dir){
    if(dir.empty()) return testSocketPath();
    if(dir.back()=='/') return dir + "helper.sock";
    return dir + "/helper.sock";
}

std::optional<Response> IpcClient::send(const Request& req, int timeoutMs){
    std::string raw = IpcProtocol::serialize(req);
    if(raw.size() > IpcProtocol::MAX_REQUEST_SIZE) return std::nullopt;
    raw += "\n";
    auto rawResp = sendRaw(raw, timeoutMs);
    if(!rawResp) return std::nullopt;
    try {
        Response resp = IpcProtocol::parseResponse(*rawResp);
        return resp;
    } catch(...){
        return std::nullopt;
    }
}

std::optional<std::string> IpcClient::sendRaw(const std::string& raw, int timeoutMs){
    if(raw.size() > IpcProtocol::MAX_REQUEST_SIZE) return std::nullopt;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd<0) return std::nullopt;
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    struct sockaddr_un addr;
    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path)-1);
    // Connect with timeout via poll
    // Set non-blocking
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    int ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret!=0 && errno!=EINPROGRESS){
        close(fd);
        return std::nullopt;
    }
    if(ret!=0){
        struct pollfd pfd; pfd.fd=fd; pfd.events=POLLOUT;
        ret = poll(&pfd, 1, timeoutMs);
        if(ret<=0){ close(fd); return std::nullopt; }
        // Check connect success
        int err=0; socklen_t len=sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if(err!=0){ close(fd); return std::nullopt; }
    }
    // Set back to blocking for send/recv but with poll timeout
    fcntl(fd, F_SETFL, flags);
    // Send
    ssize_t sent = ::send(fd, raw.c_str(), raw.size(), 0);
    if(sent != (ssize_t)raw.size()){ close(fd); return std::nullopt; }
    // Receive with poll
    struct pollfd pfd; pfd.fd=fd; pfd.events=POLLIN;
    ret = poll(&pfd, 1, timeoutMs);
    if(ret<=0){ close(fd); return std::nullopt; }
    char buf[8192];
    ssize_t n = ::recv(fd, buf, sizeof(buf)-1, 0);
    close(fd);
    if(n<=0) return std::nullopt;
    buf[n]='\0';
    std::string resp(buf, n);
    // Trim newline
    if(!resp.empty() && resp.back()=='\n') resp.pop_back();
    if(resp.size()>IpcProtocol::MAX_RESPONSE_SIZE) return std::nullopt;
    return resp;
}

} // namespace polaris::ipc
