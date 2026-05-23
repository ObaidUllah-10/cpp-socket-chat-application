cmake_minimum_required(VERSION 3.16)
project(cpp_socket_chat
        VERSION 1.0.0
        DESCRIPTION "Multi-client TCP chat application in C++"
        LANGUAGES CXX)

# ---- Standard & flags -------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# ---- Threads (pthread) ------------------------------------------------------
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ---- Shared library of common code ------------------------------------------
add_library(chatcommon STATIC
    src/Logger.cpp
    src/Socket.cpp)
target_include_directories(chatcommon PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(chatcommon PUBLIC Threads::Threads)

# ---- Server executable ------------------------------------------------------
add_executable(chat-server
    src/server_main.cpp
    src/ChatServer.cpp)
target_link_libraries(chat-server PRIVATE chatcommon)

# ---- Client executable ------------------------------------------------------
add_executable(chat-client
    src/client_main.cpp
    src/ChatClient.cpp)
target_link_libraries(chat-client PRIVATE chatcommon)

# Place binaries in build/bin for tidiness.
set_target_properties(chat-server chat-client
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
