// =============================================================================
//  ChatServer.cpp
//  ---------------------------------------------------------------------------
//  Implementation of the multi-threaded chat server.
// =============================================================================

#include "ChatServer.hpp"
#include "Logger.hpp"
#include "Protocol.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace chat {

namespace {

// Trim leading/trailing ASCII whitespace.
std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

// A username must be 1-32 chars and contain no whitespace or control chars.
bool validUsername(const std::string& name) {
    if (name.empty() || name.size() > 32) {
        return false;
    }
    for (unsigned char c : name) {
        if (c <= ' ' || c == ':' || c == 0x7f) {
            return false;
        }
    }
    return true;
}

}  // namespace

ChatServer::ChatServer(int port) : port_(port) {}

ChatServer::~ChatServer() {
    stop();
    // Join any worker threads that are still finishing up.
    std::lock_guard<std::mutex> lock(threadsMutex_);
    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

bool ChatServer::run() {
    if (!listenSocket_.create()) {
        Logger::instance().error("failed to create listening socket");
        return false;
    }
    if (!listenSocket_.bindAndListen(port_)) {
        Logger::instance().error("failed to bind/listen on port " +
                                 std::to_string(port_));
        return false;
    }

    running_ = true;
    Logger::instance().info("chat server listening on port " +
                            std::to_string(port_));

    while (running_) {
        Socket client = listenSocket_.accept();
        if (!client.isValid()) {
            if (running_) {
                Logger::instance().warn("accept() failed; continuing");
                continue;
            }
            break;  // stop() closed the listen socket: leave the loop
        }

        const std::uint64_t id   = nextId_++;
        const std::string   addr = client.peerAddress();

        auto session = std::make_shared<ClientSession>(id, std::move(client),
                                                       addr);
        session->username = "guest-" + std::to_string(id);

        addClient(session);
        Logger::instance().info("client #" + std::to_string(id) +
                                " connected from " + addr +
                                " as " + session->username);

        // Spawn a dedicated thread for this client.
        std::lock_guard<std::mutex> lock(threadsMutex_);
        workers_.emplace_back(&ChatServer::handleClient, this, session);
    }

    Logger::instance().info("accept loop terminated");
    return true;
}

void ChatServer::stop() {
    bool wasRunning = running_.exchange(false);
    if (wasRunning) {
        // Closing the listen socket unblocks the accept() call in run().
        listenSocket_.close();
        // Drop connections so per-client threads' readLine() returns.
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& [id, session] : clients_) {
            session->socket.close();
        }
    }
}

void ChatServer::handleClient(std::shared_ptr<ClientSession> session) {
    // Greet the newcomer and tell everyone else they joined.
    session->socket.sendLine("*** welcome! you are '" + session->username +
                             "'. type /help for commands.");
    broadcast("*** " + session->username + " joined the chat", session->id);

    std::string line;
    while (running_ && session->socket.readLine(line)) {
        line = trim(line);
        if (line.empty()) {
            continue;  // ignore blank lines / keepalive newlines
        }
        if (!processLine(session, line)) {
            break;  // client requested /quit
        }
    }

    // ---- Cleanup on disconnect --------------------------------------------
    const std::string name = session->username;
    removeClient(session->id);
    broadcast("*** " + name + " left the chat", session->id);
    Logger::instance().info("client #" + std::to_string(session->id) +
                            " (" + name + ") disconnected");
}

bool ChatServer::processLine(const std::shared_ptr<ClientSession>& session,
                             const std::string& line) {
    // Is this a control command (starts with '/')?
    if (line.rfind(cmd::PREFIX, 0) == 0) {
        // Split into the command word and the remainder.
        const auto spacePos = line.find(' ');
        const std::string command =
            (spacePos == std::string::npos) ? line : line.substr(0, spacePos);
        const std::string rest =
            (spacePos == std::string::npos) ? "" : trim(line.substr(spacePos + 1));

        if (command == cmd::QUIT) {
            session->socket.sendLine("*** goodbye!");
            return false;
        } else if (command == cmd::NICK) {
            cmdNick(session, rest);
        } else if (command == cmd::WHO) {
            cmdWho(session);
        } else if (command == cmd::MSG) {
            cmdMsg(session, rest);
        } else if (command == cmd::HELP) {
            cmdHelp(session);
        } else {
            session->socket.sendLine("*** unknown command '" + command +
                                     "'. type /help.");
        }
        return true;
    }

    // Ordinary chat text: broadcast it with the sender's name.
    const std::string formatted = "[" + session->username + "] " + line;
    broadcast(formatted, session->id);
    Logger::instance().info("msg " + session->username + ": " + line);
    return true;
}

// ---- Registry operations ---------------------------------------------------

void ChatServer::addClient(const std::shared_ptr<ClientSession>& session) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_[session->id] = session;
}

void ChatServer::removeClient(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.erase(id);
}

void ChatServer::broadcast(const std::string& message, std::uint64_t exceptId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& [id, session] : clients_) {
        if (id == exceptId) {
            continue;
        }
        session->socket.sendLine(message);
    }
}

bool ChatServer::sendPrivate(const std::string& targetUser,
                             const std::string& message) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& [id, session] : clients_) {
        if (session->username == targetUser) {
            session->socket.sendLine(message);
            return true;
        }
    }
    return false;
}

bool ChatServer::usernameTaken(const std::string& name) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return std::any_of(clients_.begin(), clients_.end(),
                       [&](const auto& kv) {
                           return kv.second->username == name;
                       });
}

std::string ChatServer::userList() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::ostringstream oss;
    bool first = true;
    for (auto& [id, session] : clients_) {
        if (!first) {
            oss << ", ";
        }
        oss << session->username;
        first = false;
    }
    return oss.str();
}

// ---- Command handlers -------------------------------------------------------

void ChatServer::cmdNick(const std::shared_ptr<ClientSession>& s,
                         const std::string& arg) {
    const std::string requested = trim(arg);
    if (!validUsername(requested)) {
        s->socket.sendLine("*** invalid name. use 1-32 chars, no spaces or ':'");
        return;
    }
    if (requested == s->username) {
        s->socket.sendLine("*** that is already your name");
        return;
    }
    if (usernameTaken(requested)) {
        s->socket.sendLine("*** name '" + requested + "' is taken");
        return;
    }

    // Detect whether the client is still on its auto-assigned guest name. If
    // so, this /nick is really the user choosing their initial identity, so we
    // announce it as a fresh "renamed" line rather than the noisier guest one.
    const std::string old           = s->username;
    const bool        wasGuestName  = old.rfind("guest-", 0) == 0;

    s->username = requested;
    s->socket.sendLine("*** you are now known as '" + requested + "'");

    if (wasGuestName) {
        broadcast("*** " + old + " is now '" + requested + "'", s->id);
    } else {
        broadcast("*** " + old + " is now known as '" + requested + "'", s->id);
    }
    Logger::instance().info("client #" + std::to_string(s->id) +
                            " renamed '" + old + "' -> '" + requested + "'");
}

void ChatServer::cmdWho(const std::shared_ptr<ClientSession>& s) {
    s->socket.sendLine("*** online: " + userList());
}

void ChatServer::cmdMsg(const std::shared_ptr<ClientSession>& s,
                        const std::string& rest) {
    // Expected form: "<user> <message...>"
    const auto spacePos = rest.find(' ');
    if (spacePos == std::string::npos) {
        s->socket.sendLine("*** usage: /msg <user> <message>");
        return;
    }
    const std::string target = rest.substr(0, spacePos);
    const std::string body   = trim(rest.substr(spacePos + 1));
    if (target.empty() || body.empty()) {
        s->socket.sendLine("*** usage: /msg <user> <message>");
        return;
    }
    if (target == s->username) {
        s->socket.sendLine("*** you cannot message yourself");
        return;
    }

    const std::string pm = "[pm from " + s->username + "] " + body;
    if (sendPrivate(target, pm)) {
        s->socket.sendLine("[pm to " + target + "] " + body);
        Logger::instance().info("pm " + s->username + " -> " + target);
    } else {
        s->socket.sendLine("*** no such user: " + target);
    }
}

void ChatServer::cmdHelp(const std::shared_ptr<ClientSession>& s) {
    s->socket.sendLine("*** commands:");
    s->socket.sendLine("***   /nick <name>        change your username");
    s->socket.sendLine("***   /who                list connected users");
    s->socket.sendLine("***   /msg <user> <text>  send a private message");
    s->socket.sendLine("***   /help               show this help");
    s->socket.sendLine("***   /quit               disconnect");
    s->socket.sendLine("*** anything else is broadcast to everyone.");
}

}  // namespace chat
