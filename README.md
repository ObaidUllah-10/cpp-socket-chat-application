# cpp Socket Chat Application

A multi-client TCP chat application written in modern C++ (C++17). It
demonstrates **client–server architecture**, **BSD/POSIX sockets**,
**multithreading** (one thread per client on the server, plus a dedicated
receive thread on the client), and **real-time messaging** between many
participants at once.

The protocol is plain newline-delimited UTF-8 text, so you can even talk to the
server with `telnet` or `nc` if you want to poke at it directly.

---

## Features

- **Client–server architecture** — one server, many clients.
- **Multiple simultaneous clients** — tested with 10+ concurrent connections.
- **Thread-per-client server** — each connection gets its own worker thread; a
  shared, mutex-protected registry tracks who is online.
- **Usernames** — pick one with `/nick`, with duplicate-name detection and
  validation.
- **Basic messaging** — anything you type is broadcast to everyone else.
- **Private messaging** — `/msg <user> <text>` for one-to-one chat.
- **Online user list** — `/who`.
- **Join / leave / rename notifications** broadcast to the room.
- **Thread-safe, timestamped server logs** to stderr and optionally a file.
- **Graceful shutdown** — `Ctrl-C` cleanly closes sockets and joins threads.
- **RAII socket wrapper** — descriptors are never leaked; line framing handles
  TCP's stream nature correctly.

---

## Repository layout

```
cpp-socket-chat-application/
├── include/                 # public headers
│   ├── Protocol.hpp         # shared wire-protocol constants & commands
│   ├── Logger.hpp           # thread-safe timestamped logger
│   ├── Socket.hpp           # RAII TCP socket + line framing
│   ├── ChatServer.hpp       # multi-threaded server
│   └── ChatClient.hpp       # two-threaded client
├── src/                     # implementations
│   ├── Logger.cpp
│   ├── Socket.cpp
│   ├── ChatServer.cpp
│   ├── ChatClient.cpp
│   ├── server_main.cpp      # `chat-server` entry point
│   └── client_main.cpp      # `chat-client` entry point
├── tests/
│   └── integration_test.py  # end-to-end multi-client test
├── CMakeLists.txt           # CMake build
├── Makefile                 # plain-make build
├── .gitignore
├── LICENSE
└── README.md
```

---

## Building

You need a C++17 compiler (`g++` or `clang++`) and pthreads. Works on Linux and
macOS.

### Option A — Make

```bash
make            # builds bin/chat-server and bin/chat-client
```

### Option B — CMake

```bash
cmake -B build -S .
cmake --build build
# binaries land in build/bin/
```

---

## Running

Start the server (default port 5555; optionally mirror logs to a file):

```bash
./bin/chat-server               # listen on 5555
./bin/chat-server 6000          # listen on 6000
./bin/chat-server 6000 chat.log # also write logs to chat.log
```

In other terminals, start one or more clients:

```bash
./bin/chat-client                          # connect to 127.0.0.1:5555
./bin/chat-client 127.0.0.1 5555 alice     # connect with username "alice"
./bin/chat-client chat.example.com 6000 bob
```

Then just type and press Enter to chat.

---

## Commands

Type these inside a connected client:

| Command                | Description                          |
|------------------------|--------------------------------------|
| `/nick <name>`         | Change your username                 |
| `/who`                 | List everyone currently online       |
| `/msg <user> <text>`   | Send a private message               |
| `/help`                | Show the command list                |
| `/quit`                | Disconnect                           |

Anything that does not start with `/` is broadcast to all other users.

---

## How it works

### Server (`ChatServer`)

The main thread runs an `accept()` loop. For every new connection it creates a
`ClientSession` (socket + username + id), registers it in a mutex-protected map,
and spawns a dedicated worker thread (`handleClient`). That thread reads framed
lines from its client, dispatches commands or broadcasts chat text, and on
disconnect removes the session and announces the departure. Broadcasting walks
the registry under the lock and writes to every other client's socket.

### Client (`ChatClient`)

The client runs two threads. The **main thread** reads keyboard input with
`std::getline` and sends each line. A **receive thread** blocks on the socket
and prints incoming messages as they arrive, so you see other people's messages
in real time even while typing.

### Framing

TCP is a byte stream, not a message stream — a single `recv()` may return a
partial line or several lines glued together. `Socket::readLine()` buffers bytes
and hands back exactly one `\n`-terminated line at a time, which is what makes
the chat protocol reliable.

---

## Testing

An end-to-end integration test spins up the server and drives several raw socket
clients through it:

```bash
make
python3 tests/integration_test.py
```

It verifies welcome/nick handling, join notifications, broadcast delivery,
`/who`, private messaging, duplicate-username rejection, and leave
notifications.

---

## Possible extensions

- TLS via OpenSSL for encrypted transport.
- Chat rooms / channels.
- Message history persistence.
- An `epoll`/`kqueue` event loop instead of thread-per-client to scale to
  thousands of connections.
- A length-prefixed binary protocol for non-text payloads.

---

## License

MIT — see [LICENSE](LICENSE).
