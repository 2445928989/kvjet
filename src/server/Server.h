// Server.h
#pragma once
#include "../util/HashTable.h"
#include "../util/Socket.h"
#include "../util/ThreadPool.h"
#include <cerrno>
#include <cstdint>
#include <optional>
#include <string>
// 服务端类
class Server {
public:
    // 构造函数，传入一个端口
    Server(uint16_t port);
    // 析构函数
    ~Server();

    // 禁止拷贝
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    // 发送消息，返回长度，不成功时返回-1
    ssize_t send(const std::string &str);
    // 接收消息
    std::string recv();

    // 运行循环
    void run();

private:
    Socket server_sock;
    // 延迟初始化
    std::optional<Socket> client_sock;

    ThreadPool threadPool;
};