// Cluster.h
#pragma once
#include "../util/Socket.h"
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <thread>
class Cluster {
public:
    Cluster();
    explicit Cluster(uint16_t heartbeat_port, const std::string &ip);
    ~Cluster();
    struct Heartbeat {
        uint64_t node_id;
        uint64_t timestamp;
        Heartbeat(uint64_t node_id) : node_id(node_id), timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) {}
    };
    struct Node {
        std::string ip;
        uint16_t port;
        uint64_t UUID;
    };
    // 用于收发心跳包的循环
    void heartbeatLoop();
    // 添加一个Node
    void addNodeToHash(uint64_t node_id);
    // 删除一个Node
    void delNodeToHash(uint64_t node_id);
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
    bool findGossip(uint64_t UUID);
    void addGossip(uint64_t UUID);
    std::map<uint64_t, Node> getTopo() {
        return topo;
    }
    std::map<uint64_t, int> getConnections() {
        return connections;
    }
    void delTopoNode(uint64_t UUID);
    void addTopoNode(Node node);
    void addConnection(uint64_t UUID, int fd);
    void delConnection(uint64_t UUID);
    int getConnection(uint64_t UUID);
    const Node &getSelf() { return self_node; }
    uint64_t generateUUID();

private:
    volatile bool running = false;
    Node self_node;
    std::thread heartbeat_thread;
    std::shared_mutex node_hash_lock;
    std::shared_mutex gossip_cache_lock;
    std::shared_mutex heartbeat_time_lock;
    std::shared_mutex connections_lock;
    std::shared_mutex topo_lock;
    // 维护一致性哈希
    std::map<uint64_t, uint64_t> node_hash;
    // 维护每个节点上次收到心跳包的时间戳
    std::map<uint64_t, uint64_t> heartbeat_time;
    // 维护node_id到fd的映射
    std::map<uint64_t, int> connections;
    // 完整的网络拓扑
    std::map<uint64_t, Node> topo;
    int heartbeat_listensock;
    int epoll_fd;
    std::map<uint64_t, uint64_t> gossip_cache;
};