cmake_minimum_required(VERSION 3.10)
project(kvjet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译优化和警告
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# 如果没有指定构建类型，默认为Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 头文件包含路径
include_directories(${PROJECT_SOURCE_DIR}/src)

# 实用工具源文件
set(UTIL_SOURCES
    src/util/Socket.cpp
    src/util/ThreadPool.cpp
    src/util/HashTable.cpp
    src/util/KVStore.cpp
    src/util/HashTable1.cpp
    src/util/KVStore1.cpp
    src/util/AOF.cpp
    src/util/LRU.cpp
)

# RESP协议源文件
set(RESP_SOURCES
    src/resp/RespValue.cpp
    src/resp/RespParser.cpp
    src/resp/RespEncoder.cpp
)

# 服务端处理源文件
set(HANDLER_SOURCES
    src/server/Handler.cpp
)

# 客户端源文件
set(CLIENT_SOURCES
    src/client/Client.cpp
    ${UTIL_SOURCES}
    ${RESP_SOURCES}
)

# 服务端源文件
set(SERVER_SOURCES
    src/server/Server.cpp
    ${HANDLER_SOURCES}
    ${UTIL_SOURCES}
    ${RESP_SOURCES}
)

find_package(Threads REQUIRED)

# 查找 jemalloc
find_package(jemalloc CONFIG QUIET)
if(NOT JEMALLOC_FOUND)
    # 如果没有cmake config，尝试直接找库
    set(JEMALLOC_LIBRARIES jemalloc)
endif()

# 编译标志统一应用
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# Server 二进制文件
add_executable(kvserver
    src/server_main.cpp
    ${SERVER_SOURCES}
)
target_link_libraries(kvserver PRIVATE Threads::Threads ${JEMALLOC_LIBRARIES})
target_include_directories(kvserver PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Client 二进制文件
add_executable(kvclient
    src/client_main.cpp
    ${CLIENT_SOURCES}
)
target_link_libraries(kvclient PRIVATE Threads::Threads ${JEMALLOC_LIBRARIES})
target_include_directories(kvclient PRIVATE ${PROJECT_SOURCE_DIR}/src)



# 测试暂不可用


# # Test executable
# add_executable(kvtest
#     tests/test_server.cpp
#     ${SERVER_SOURCES}
# )
# target_link_libraries(kvtest PRIVATE Threads::Threads)
# target_include_directories(kvtest PRIVATE ${PROJECT_SOURCE_DIR}/src)

# # KVStore unit test executable
# add_executable(kvtest_kvstore
#     tests/test_KVStore.cpp
#     ${UTIL_SOURCES}
# )
# target_link_libraries(kvtest_kvstore PRIVATE Threads::Threads)
# target_include_directories(kvtest_kvstore PRIVATE ${PROJECT_SOURCE_DIR}/src)

# # RESP protocol unit test executable
# add_executable(kvtest_resp
#     tests/test_RESP.cpp
#     ${RESP_SOURCES}
# )
# target_link_libraries(kvtest_resp PRIVATE Threads::Threads)
# target_include_directories(kvtest_resp PRIVATE ${PROJECT_SOURCE_DIR}/src)

# # Enable testing
# enable_testing()

# # Add test case with automatic server startup
# add_test(
#     NAME test_echo_1
#     COMMAND bash -c "
#         $<TARGET_FILE:kvserver> 9999 &
#         SERVER_PID=\$!
#         sleep 1
#         $<TARGET_FILE:kvtest> 127.0.0.1 9999 \
#             ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cases/1.in \
#             ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cases/1.out
#         TEST_RESULT=\$?
#         kill \$SERVER_PID 2>/dev/null
#         exit \$TEST_RESULT
#     "
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
# )

# # Add KvStore unit test
# add_test(
#     NAME test_kvstore_unit
#     COMMAND $<TARGET_FILE:kvtest_kvstore>
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
# )

# # Add RESP protocol unit test
# add_test(
#     NAME test_resp_unit
#     COMMAND $<TARGET_FILE:kvtest_resp>
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
# )

# add_executable(testaof
#     tests/test_AOF.cpp
#     ${UTIL_SOURCES}
# )
# target_link_libraries(testaof PRIVATE Threads::Threads)

# add_test(
#     NAME test_AOF_unit
#     COMMAND $<TARGET_FILE:testaof>
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
# )