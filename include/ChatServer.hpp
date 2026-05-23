#ifndef CHAT_SERVER_HPP
#define CHAT_SERVER_HPP

// =============================================================================
//  ChatServer.hpp
//  ---------------------------------------------------------------------------
//  The multi-client chat server. Architecture:
//
//      main thread          ── accept() loop, spawns one thread per client
//      per-client threads   ── read lines from their socket, act on them
//      shared ClientTable   ── registry of connected clients, mutex-protected
//
//  Each connected client is represented by a ClientSession holding its socket,
//  chosen username, and a unique id. Broadcasting a message walks the registry
//  and writes to every *other* client's socket. All access to the registry is
//  guarded by a single mutex, which is sufficient and simple for a chat server
//  of this scale.
// =============================================================================

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Socket.hpp"

namespace chat {

// One connected participant.
struct ClientSession {
    std::uint64_t id;        // unique, monotonically increasing
    Socket        socket;    // owned connection to this client
    std::string   username;  // display name (defaults to "guest-<id>")
    std::string   address;   // remote ip:port, captured at accept time

    ClientSession(std::uint64_t i, Socket&& s, std::string addr)
        : id(i), socket(std::move(s)), address(std::move(addr)) {}
};

class ChatServer {
public:
    explicit ChatServer(int port);
    ~ChatServer();

    // Bind, listen, and run the accept loop. Blocks until stop() is called or
    // the listening socket fails. Returns false if start-up failed.
    bool run();

    // Ask the server to shut down (safe to call from a signal handler-ish
    // context: it only flips an atomic flag and closes the listen socket).
    void stop();

    ChatServer(const ChatServer&)            = delete;
    ChatServer& operator=(const ChatServer&) = delete;

private:
    // Thread body: owns one ClientSession for its whole lifetime.
    void handleClient(std::shared_ptr<ClientSession> session);

    // Command/message dispatch for a single received line.
    // Returns false if the client should be disconnected afterwards.
    bool processLine(const std::shared_ptr<ClientSession>& session,
                     const std::string& line);

    // ---- Registry operations (all lock clientsMutex_) ----------------------
    void addClient(const std::shared_ptr<ClientSession>& session);
    void removeClient(std::uint64_t id);

    // Send `message` to everyone except `exceptId` (use 0 to include all).
    void broadcast(const std::string& message, std::uint64_t exceptId);

    // Send `message` privately to the client whose username matches (case
    // sensitive). Returns false if no such user is connected.
    bool sendPrivate(const std::string& targetUser, const std::string& message);

    // True if `name` is already taken by a connected client.
    bool usernameTaken(const std::string& name);

    // Comma-separated list of currently connected usernames.
    std::string userList();

    // ---- Command handlers --------------------------------------------------
    void cmdNick(const std::shared_ptr<ClientSession>& s, const std::string& arg);
    void cmdWho (const std::shared_ptr<ClientSession>& s);
    void cmdMsg (const std::shared_ptr<ClientSession>& s, const std::string& rest);
    void cmdHelp(const std::shared_ptr<ClientSession>& s);

    int                       port_;
    Socket                    listenSocket_;
    std::atomic<bool>         running_{false};
    std::atomic<std::uint64_t> nextId_{1};

    std::mutex clientsMutex_;
    std::map<std::uint64_t, std::shared_ptr<ClientSession>> clients_;

    // Detached worker threads are tracked so we can join on shutdown.
    std::mutex               threadsMutex_;
    std::vector<std::thread> workers_;
};

}  // namespace chat

#endif  // CHAT_SERVER_HPP
