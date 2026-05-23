// =============================================================================
//  server_main.cpp
//  ---------------------------------------------------------------------------
//  Entry point for the chat server executable.
//
//  Usage:
//      chat-server [port] [logfile]
//
//      port     TCP port to listen on   (default: 5555)
//      logfile  optional path; if given, logs are mirrored to this file
//
//  A SIGINT (Ctrl-C) handler triggers a graceful shutdown: the listening
//  socket is closed, all client connections are dropped, and worker threads
//  are joined before the process exits.
// =============================================================================

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

#include "ChatServer.hpp"
#include "Logger.hpp"
#include "Protocol.hpp"

namespace {
// The signal handler can only touch async-signal-safe state, so it just points
// at the live server and asks it to stop.
chat::ChatServer* g_server = nullptr;

void handleSignal(int /*signum*/) {
    if (g_server) {
        g_server->stop();
    }
}
}  // namespace

int main(int argc, char* argv[]) {
    int         port    = chat::DEFAULT_PORT;
    std::string logfile;

    if (argc >= 2) {
        try {
            port = std::stoi(argv[1]);
        } catch (const std::exception&) {
            std::cerr << "invalid port: " << argv[1] << '\n';
            return EXIT_FAILURE;
        }
    }
    if (argc >= 3) {
        logfile = argv[2];
    }

    if (!logfile.empty()) {
        chat::Logger::instance().enableFileLogging(logfile);
    }

    chat::ChatServer server(port);
    g_server = &server;

    // Install graceful-shutdown handlers.
    std::signal(SIGINT,  handleSignal);
    std::signal(SIGTERM, handleSignal);

    if (!server.run()) {
        chat::Logger::instance().error("server failed to start");
        return EXIT_FAILURE;
    }

    chat::Logger::instance().info("server stopped cleanly");
    return EXIT_SUCCESS;
}
