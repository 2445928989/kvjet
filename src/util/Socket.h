// Socket.h
#pragma once
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// 封装版 socket，RAII
// 不可拷贝，析构时自动释放描述符
// 异常时抛出 runtime_error
// 暂不支持ipv6
class Socket {
public:
    // 构造函数，默认ipv4 TCP协议
    Socket();

    // 构造函数，指定域、类型、协议
    Socket(int domain, int type, int protocol);

    // 析毁函数，自动释放描述符
    ~Socket() noexcept;

    // 禁止拷贝
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

    // 允许move
    Socket(Socket &&other) noexcept;
    Socket &operator=(Socket &&other) noexcept;

    // 获取描述符
    int fd() const;

    // 绑定socket，传入ip 0.0.0.0以接受所有请求
    void bind(const std::string &ip, uint16_t port);

    // socket连接，支持域名和ip解析(ipv4)
    void connect(const std::string &ip, uint16_t port);

    // 打开监听模式，开始接收客户端连接
    void listen(int backlog = 128);

    // 接受客户端连接
    Socket accept();

private:
    explicit Socket(int fd) noexcept;
    int fd_;
};