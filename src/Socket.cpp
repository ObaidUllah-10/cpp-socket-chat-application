// =============================================================================
//  Socket.cpp
//  ---------------------------------------------------------------------------
//  Implementation of the RAII Socket wrapper. Uses the BSD/POSIX sockets API
//  (sys/socket.h, netinet/in.h, arpa/inet.h). Compiles on Linux and macOS.
// =============================================================================

#include "Socket.hpp"
#include "Protocol.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <utility>

namespace chat {

Socket::Socket(int fd) : fd_(fd) {}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept
    : fd_(other.fd_), inbuf_(std::move(other.inbuf_)) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_       = other.fd_;
        inbuf_    = std::move(other.inbuf_);
        other.fd_ = -1;
    }
    return *this;
}

bool Socket::create() {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    return fd_ >= 0;
}

bool Socket::connectTo(const std::string& host, int port) {
    if (fd_ < 0 && !create()) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));

    // Try to interpret `host` as a dotted-quad first; if that fails, resolve
    // it via getaddrinfo so hostnames like "localhost" work too.
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        addrinfo hints{};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (::getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) {
            return false;
        }
        addr.sin_addr =
            reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
        ::freeaddrinfo(res);
    }

    return ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

bool Socket::bindAndListen(int port, int backlog) {
    if (fd_ < 0 && !create()) {
        return false;
    }

    // Allow immediate reuse of the port after the server restarts, avoiding
    // the "Address already in use" error during the TIME_WAIT window.
    int opt = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port));

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        return false;
    }
    return ::listen(fd_, backlog) == 0;
}

Socket Socket::accept() {
    sockaddr_in peer{};
    socklen_t   len = sizeof(peer);
    int clientFd = ::accept(fd_, reinterpret_cast<sockaddr*>(&peer), &len);
    return Socket(clientFd);  // invalid Socket if clientFd < 0
}

bool Socket::sendLine(const std::string& message) {
    if (fd_ < 0) {
        return false;
    }

    std::string data = message;
    if (data.empty() || data.back() != '\n') {
        data.push_back('\n');
    }

    std::size_t totalSent = 0;
    while (totalSent < data.size()) {
        // MSG_NOSIGNAL stops a write to a closed socket from killing us with
        // SIGPIPE; we detect the error via the return value instead.
#if defined(MSG_NOSIGNAL)
        ssize_t n = ::send(fd_, data.data() + totalSent,
                           data.size() - totalSent, MSG_NOSIGNAL);
#else
        ssize_t n = ::send(fd_, data.data() + totalSent,
                           data.size() - totalSent, 0);
#endif
        if (n <= 0) {
            if (n < 0 && errno == EINTR) {
                continue;  // interrupted by a signal: retry
            }
            return false;  // peer closed or fatal error
        }
        totalSent += static_cast<std::size_t>(n);
    }
    return true;
}

bool Socket::readLine(std::string& out) {
    // First, see if a complete line is already sitting in the buffer.
    for (;;) {
        auto pos = inbuf_.find('\n');
        if (pos != std::string::npos) {
            out = inbuf_.substr(0, pos);
            // Strip a trailing '\r' so CRLF clients (telnet) behave.
            if (!out.empty() && out.back() == '\r') {
                out.pop_back();
            }
            inbuf_.erase(0, pos + 1);
            return true;
        }

        // Guard against an unbounded line from a misbehaving peer.
        if (inbuf_.size() > MAX_LINE_LENGTH) {
            return false;
        }

        // No full line yet: pull more bytes off the socket.
        char buf[RECV_BUFFER_SIZE];
        ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
        if (n == 0) {
            return false;  // peer performed an orderly shutdown
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;  // retry on interrupt
            }
            return false;  // fatal error
        }
        inbuf_.append(buf, static_cast<std::size_t>(n));
    }
}

void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

std::string Socket::peerAddress() const {
    if (fd_ < 0) {
        return "unknown";
    }
    sockaddr_in addr{};
    socklen_t   len = sizeof(addr);
    if (::getpeername(fd_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        return "unknown";
    }
    char ip[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

}  // namespace chat
