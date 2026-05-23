#ifndef SOCKET_HPP
#define SOCKET_HPP

// =============================================================================
//  Socket.hpp
//  ---------------------------------------------------------------------------
//  A thin RAII wrapper around a POSIX TCP socket file descriptor. It owns the
//  descriptor (closing it in the destructor), provides connect/bind/listen/
//  accept helpers, and implements newline-delimited message framing on top of
//  the raw byte stream.
//
//  Why framing matters
//  -------------------
//  TCP is a *stream* protocol: a single recv() may return half a message, one
//  whole message, or several messages glued together. To recover discrete chat
//  lines we buffer incoming bytes and hand back complete '\n'-terminated lines
//  one at a time via readLine().
// =============================================================================

#include <cstdint>
#include <string>

namespace chat {

class Socket {
public:
    // Construct an empty (invalid) socket.
    Socket() = default;

    // Adopt an already-open file descriptor (e.g. the result of accept()).
    explicit Socket(int fd);

    // Destructor closes the underlying descriptor if still open.
    ~Socket();

    // Movable but not copyable: a descriptor must have exactly one owner.
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    Socket(const Socket&)            = delete;
    Socket& operator=(const Socket&) = delete;

    // ---- Connection setup --------------------------------------------------

    // Create the underlying TCP socket. Returns false on failure.
    bool create();

    // Client side: connect to host:port. Returns false on failure.
    bool connectTo(const std::string& host, int port);

    // Server side: bind to port (all interfaces) and start listening.
    bool bindAndListen(int port, int backlog = 16);

    // Server side: block until a client connects; returns the new connection.
    // On error the returned Socket is invalid (isValid() == false).
    Socket accept();

    // ---- I/O ---------------------------------------------------------------

    // Send an entire string, retrying until all bytes are written.
    // A trailing '\n' is appended if not already present. Returns false if the
    // peer closed the connection or an unrecoverable error occurred.
    bool sendLine(const std::string& message);

    // Read a single newline-terminated line into `out` (newline stripped).
    // Returns false when the connection is closed or an error occurs.
    bool readLine(std::string& out);

    // ---- State -------------------------------------------------------------

    bool isValid() const { return fd_ >= 0; }
    int  fd()      const { return fd_; }
    void close();

    // Human-readable "ip:port" of the connected peer (best effort).
    std::string peerAddress() const;

private:
    int         fd_      = -1;   // -1 means "no socket"
    std::string inbuf_;          // accumulates bytes for line framing
};

}  // namespace chat

#endif  // SOCKET_HPP
