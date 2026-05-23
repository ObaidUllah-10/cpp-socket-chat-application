// =============================================================================
//  ChatClient.cpp
//  ---------------------------------------------------------------------------
//  Implementation of the two-threaded chat client.
// =============================================================================

#include "ChatClient.hpp"
#include "Protocol.hpp"

#include <iostream>
#include <utility>

namespace chat {

ChatClient::ChatClient(std::string host, int port, std::string username)
    : host_(std::move(host)), port_(port), username_(std::move(username)) {}

ChatClient::~ChatClient() {
    running_ = false;
    socket_.close();
    if (receiver_.joinable()) {
        receiver_.join();
    }
}

bool ChatClient::run() {
    if (!socket_.create() || !socket_.connectTo(host_, port_)) {
        std::cerr << "error: could not connect to " << host_ << ':' << port_
                  << '\n';
        return false;
    }

    running_ = true;
    std::cout << "connected to " << host_ << ':' << port_ << '\n';

    // If the user supplied a username up front, register it immediately.
    if (!username_.empty()) {
        socket_.sendLine(std::string(cmd::NICK) + " " + username_);
    }

    // Start the background thread that prints messages from the server.
    receiver_ = std::thread(&ChatClient::receiveLoop, this);

    // Main thread: read keyboard input line by line and send it.
    std::string input;
    while (running_ && std::getline(std::cin, input)) {
        if (!socket_.sendLine(input)) {
            std::cerr << "error: connection lost while sending\n";
            break;
        }
        // Locally honour /quit so we stop reading stdin right away.
        if (input == cmd::QUIT) {
            break;
        }
    }

    running_ = false;
    socket_.close();
    if (receiver_.joinable()) {
        receiver_.join();
    }
    std::cout << "disconnected.\n";
    return true;
}

void ChatClient::receiveLoop() {
    std::string line;
    while (running_ && socket_.readLine(line)) {
        // The "\r" carriage return + flush keeps output tidy if the user is
        // mid-line; the message prints on its own line.
        std::cout << line << '\n';
        std::cout.flush();
    }

    // If the server closed the connection, nudge the main thread out of its
    // blocking getline by reporting the drop. (The user still needs to press
    // Enter once to fully exit, a known limitation of blocking std::getline.)
    if (running_) {
        running_ = false;
        std::cout << "*** server closed the connection. press Enter to exit.\n";
        std::cout.flush();
    }
}

}  // namespace chat
