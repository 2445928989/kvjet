// Client.h
#pragma once
#include "../util/Socket.h"
#include <cerrno>
#include <cstdint>
#include <string>
// 客户端类
class Client {
public:
    // 构造函数
    Client(const std::string &ip, uint16_t port);
    // 析构函数
    ~Client();
    // 禁止拷贝
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
    // 发送请求，返回长度，不成功时返回-1
    ssize_t send(const std::string &request);
    // 接收消息
    std::string recv();
    // 运行循环
    void run();
    // 压测模式
    void benchmark(int ops, const std::string &op_type);
    // 处理指令
    // 指令错误时返回一个resp::Error
    resp::RespValue handle(std::string);

private:
    Socket sock;
};