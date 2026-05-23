#ifndef CHAT_CLIENT_HPP
#define CHAT_CLIENT_HPP

// =============================================================================
//  ChatClient.hpp
//  ---------------------------------------------------------------------------
//  The chat client. It connects to a server and then runs two threads:
//
//      receive thread  ── blocks on the socket, prints incoming lines
//      main thread     ── reads the user's keyboard input, sends it on
//
//  Splitting send and receive across threads lets the user see messages from
//  others arrive in real time while they are still typing, which is the whole
//  point of a chat client.
// =============================================================================

#include <atomic>
#include <string>
#include <thread>

#include "Socket.hpp"

namespace chat {

class ChatClient {
public:
    ChatClient(std::string host, int port, std::string username);
    ~ChatClient();

    // Connect and run the input loop. Blocks until the user quits or the
    // connection drops. Returns false if the initial connection failed.
    bool run();

    ChatClient(const ChatClient&)            = delete;
    ChatClient& operator=(const ChatClient&) = delete;

private:
    // Body of the background receive thread.
    void receiveLoop();

    std::string       host_;
    int               port_;
    std::string       username_;
    Socket            socket_;
    std::atomic<bool> running_{false};
    std::thread       receiver_;
};

}  // namespace chat

#endif  // CHAT_CLIENT_HPP
