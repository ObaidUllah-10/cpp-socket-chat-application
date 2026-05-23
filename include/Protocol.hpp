#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

// =============================================================================
//  Protocol.hpp
//  ---------------------------------------------------------------------------
//  Shared wire-protocol constants and helpers used by both the chat server and
//  the chat client. Keeping this in one header guarantees that both ends speak
//  exactly the same language.
//
//  Wire format
//  -----------
//  Every logical message is a single line of UTF-8 text terminated by '\n'.
//  This keeps the protocol trivial to debug (you can talk to the server with
//  `telnet` or `nc`) while still being unambiguous to frame.
//
//  Control messages sent from the client to the server are prefixed with '/'
//  (e.g. "/nick alice", "/quit"). Anything that does not start with '/' is
//  treated as ordinary chat text and broadcast to every other participant.
// =============================================================================

#include <string>

namespace chat {

// ---- Network defaults -------------------------------------------------------
constexpr int          DEFAULT_PORT     = 5555;
constexpr const char*  DEFAULT_HOST     = "127.0.0.1";

// Maximum number of bytes we will read in a single recv() call.
constexpr int          RECV_BUFFER_SIZE = 4096;

// Maximum length we allow for a single framed line. Protects the server from a
// malicious client that never sends a newline.
constexpr std::size_t  MAX_LINE_LENGTH  = 8192;

// ---- Command tokens (client -> server) -------------------------------------
namespace cmd {
constexpr const char* PREFIX = "/";
constexpr const char* NICK   = "/nick";   // change username:  /nick <name>
constexpr const char* QUIT   = "/quit";   // disconnect gracefully
constexpr const char* WHO    = "/who";    // list connected users
constexpr const char* MSG    = "/msg";    // private message:  /msg <user> <text>
constexpr const char* HELP   = "/help";   // show available commands
}  // namespace cmd

}  // namespace chat

#endif  // PROTOCOL_HPP
