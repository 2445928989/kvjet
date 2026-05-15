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
    // 发送请求，is_read 决定路由策略
    ssize_t send(const std::string &request, const std::string &key, bool is_read);
    // 接收消息，指定 fd（与 send 返回的路由一致）
    std::string recv(int target_fd);
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
    // 判断命令类型
    static bool isReadCommand(const std::string &cmd);
    static bool isWriteCommand(const std::string &cmd);
    // 根据命令和 key 决定路由目标
    uint64_t routeTarget(const std::string &cmd, const std::string &key);
    // 读写路由：读随机，写走哈希环
    uint64_t routeRead(const std::string &key);
    uint64_t routeWrite(const std::string &key);

    // MOVED 处理和拓扑刷新
    static bool isMovedResponse(const std::string &resp);
    void refreshTopology();

private:
    Cluster cluster;
    std::map<int, Socket> connections;
    int last_sent_fd = -1;
};