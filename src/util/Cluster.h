// Cluster.h
#pragma once
#include "../util/Socket.h"
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
class Cluster {
public:
    Cluster();
    ~Cluster();
    struct Heartbeat {
        uint64_t node_id;
        uint64_t timestamp;
        Heartbeat(uint64_t node_id) : node_id(node_id), timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) {}
    };
    // 用于收发心跳包的循环
    void heartbeatLoop();
    // 添加一个Node
    void addNode(uint64_t node_id);
    // 删除一个Node
    void delNode(uint64_t node_id);
    // 查询需要询问的节点
    uint64_t queryNode(const std::string &str);
    // 哈希节点
    uint64_t hash(uint64_t x);
    // 哈希字符串
    uint64_t hash(const std::string &str);
    // 是否在线
    bool isAlive(uint64_t node_id);
    // 启动心跳和gossip线程
    void start();
    // 停止线程
    void stop();
    // 初始化心跳
    void heartbeatInit();

private:
    volatile bool running = false;
    std::thread heartbeat_thread;
    std::mutex node_map_lock;
    // 维护一致性哈希
    std::map<uint64_t, uint64_t> node_map;
    // 维护每个节点上次收到心跳包的时间戳
    std::map<uint64_t, uint64_t> heartbeat_time;
    // 维护node_id到fd的映射
    std::map<uint64_t, int> connections;
    int heartbeat_listensock;
    int epoll_fd;
    std::map<uint64_t, uint64_t> msg_cache;
};