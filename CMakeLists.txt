cmake_minimum_required(VERSION 3.10)
project(kvjet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 实用工具源文件
set(UTIL_SOURCES
    src/util/Socket.cpp
    src/util/ThreadPool.cpp
    src/util/HashTable.cpp
)

# RESP协议源文件
set(RESP_SOURCES
    src/resp/RespValue.cpp
    src/resp/RespParser.cpp
    src/resp/RespEncoder.cpp
)

set(CLIENT_SOURCES
    src/client/Client.cpp
    ${UTIL_SOURCES}
    ${RESP_SOURCES}
)

set(SERVER_SOURCES
    src/server/Server.cpp
    ${UTIL_SOURCES}
    ${RESP_SOURCES}
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
    tests/test_server.cpp
    ${CLIENT_SOURCES}
)
target_link_libraries(kvtest PRIVATE Threads::Threads)

# HashTable unit test executable
add_executable(kvtest_hashtable
    tests/test_hashtable.cpp
    ${UTIL_SOURCES}
)
target_link_libraries(kvtest_hashtable PRIVATE Threads::Threads)

# Enable testing
enable_testing()

# Add test case with automatic server startup
add_test(
    NAME test_echo_1
    COMMAND bash -c "
        $<TARGET_FILE:kvserver> 9999 &
        SERVER_PID=\$!
        sleep 1
        $<TARGET_FILE:kvtest> 127.0.0.1 9999 \
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cases/1.in \
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cases/1.out
        TEST_RESULT=\$?
        kill \$SERVER_PID 2>/dev/null
        exit \$TEST_RESULT
    "
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add HashTable unit test
add_test(
    NAME test_hashtable_unit
    COMMAND $<TARGET_FILE:kvtest_hashtable>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
