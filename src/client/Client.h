// Client.h
#pragma once
#include "../util/Cluster.h"
#include "../util/Socket.h"
#include <cerrno>
#include <cstdint>
#include <map>
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
    ssize_t send(const std::string &request, const std::string &key);
    // 接收消息
    std::string recv(const std::string &key);
    // 辅助方法：获取Socket引用
    Socket &getSocket(int fd);
    // 运行循环
    void run();
    // 压测模式
    void benchmark(int ops, const std::string &op_type);
    // 延迟测量模式
    void latencyBenchmark(int ops, const std::string &op_type);
    // 处理指令
    // 指令错误时返回一个resp::Error
    resp::RespValue handle(std::string);
    // 从RespValue中提取key（用于路由）
    std::string extractKeyFromRequest(const resp::RespValue &req);

private:
    Cluster cluster;
    std::map<int, Socket> connections;
};