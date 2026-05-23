// =============================================================================
//  client_main.cpp
//  ---------------------------------------------------------------------------
//  Entry point for the chat client executable.
//
//  Usage:
//      chat-client [host] [port] [username]
//
//      host      server address     (default: 127.0.0.1)
//      port      server TCP port    (default: 5555)
//      username  initial nickname   (default: server-assigned guest name)
//
//  Once connected, type messages and press Enter to send. Lines beginning with
//  '/' are commands (type /help once connected for the full list).
// =============================================================================

#include <cstdlib>
#include <iostream>
#include <string>

#include "ChatClient.hpp"
#include "Protocol.hpp"

int main(int argc, char* argv[]) {
    std::string host     = chat::DEFAULT_HOST;
    int         port     = chat::DEFAULT_PORT;
    std::string username;

    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        try {
            port = std::stoi(argv[2]);
        } catch (const std::exception&) {
            std::cerr << "invalid port: " << argv[2] << '\n';
            return EXIT_FAILURE;
        }
    }
    if (argc >= 4) {
        username = argv[3];
    }

    chat::ChatClient client(host, port, username);
    if (!client.run()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
