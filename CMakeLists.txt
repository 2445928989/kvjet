cmake_minimum_required(VERSION 3.10)
project(kvjet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOCKET_SOURCES
    src/util/Socket.cpp
)

set(CLIENT_SOURCES
    src/client/Client.cpp
    ${SOCKET_SOURCES}
)

set(SERVER_SOURCES
    src/server/Server.cpp
    ${SOCKET_SOURCES}
)

# Link pthread
find_package(Threads REQUIRED)

# Server executable
add_executable(kvserver
    src/server_main.cpp
    ${SERVER_SOURCES}
)
target_link_libraries(kvserver PRIVATE Threads::Threads)

# Client executable
add_executable(kvclient
    src/client_main.cpp
    ${CLIENT_SOURCES}
)
target_link_libraries(kvclient PRIVATE Threads::Threads)

# Test executable
add_executable(kvtest
    tests/test.cpp
    ${CLIENT_SOURCES}
)
target_link_libraries(kvtest PRIVATE Threads::Threads)

# Enable testing
enable_testing()

# Add test case
add_test(
    NAME test_echo_1
    COMMAND kvtest 127.0.0.1 9999
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cases/1.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cases/1.out"
)
