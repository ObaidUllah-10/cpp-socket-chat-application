# =============================================================================
#  Makefile
#  ---------------------------------------------------------------------------
#  A plain-make alternative to CMake. Builds two binaries into ./bin:
#
#      make            build both chat-server and chat-client
#      make server     build only the server
#      make client     build only the client
#      make clean      remove build artifacts
#
#  Requires a C++17 compiler (g++ or clang++) and pthreads.
# =============================================================================

CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS  := -pthread

BIN_DIR  := bin
OBJ_DIR  := obj

# Code shared by both executables.
COMMON_SRC := src/Logger.cpp src/Socket.cpp
COMMON_OBJ := $(COMMON_SRC:src/%.cpp=$(OBJ_DIR)/%.o)

SERVER_SRC := src/server_main.cpp src/ChatServer.cpp
SERVER_OBJ := $(SERVER_SRC:src/%.cpp=$(OBJ_DIR)/%.o)

CLIENT_SRC := src/client_main.cpp src/ChatClient.cpp
CLIENT_OBJ := $(CLIENT_SRC:src/%.cpp=$(OBJ_DIR)/%.o)

SERVER_BIN := $(BIN_DIR)/chat-server
CLIENT_BIN := $(BIN_DIR)/chat-client

.PHONY: all server client clean

all: server client

server: $(SERVER_BIN)
client: $(CLIENT_BIN)

$(SERVER_BIN): $(COMMON_OBJ) $(SERVER_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(CLIENT_BIN): $(COMMON_OBJ) $(CLIENT_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Pattern rule: compile any src/*.cpp into obj/*.o
$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
