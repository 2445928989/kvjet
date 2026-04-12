// Server.h
#pragma once
#include "../config/Config.h"
#include "../kvstore/AOF.h"
#include "../kvstore/KVStore.h"
#include "../resp/RespValue.h"
#include "../util/Cluster.h"
#include "../util/Socket.h"
#include "../util/ThreadPool.h"
#include <cerrno>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <sys/epoll.h>
// 服务端类
// 规定message_queue中fd为-1的是指示服务端主动连接某个服务器
// 其格式为ip port UUID
class Server {
public:
    // 构造函数，传入一个端口和ip地址
    Server(const std::string &ip, uint16_t port);
    // 析构函数
    ~Server();

    // 禁止拷贝
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    // 发送消息，返回长度，不成功时返回-1
    ssize_t send(const std::string &str, const Socket &sock);

    // 接收消息
    std::string recv(const Socket &sock);

    // 处理来自socket的请求
    void handleCommand(int sock, resp::RespValue value);

    // 接受socket连接
    bool accept();

    // 主动连接到其他服务器
    void connect(const std::string &ip, uint16_t port, uint64_t uuid);

    // 发送所有属于目标节点的kv数据到该节点
    void sendDataToNode(uint64_t target_uuid);

    // 加入一个已存在的集群
    void joinCluster(const std::string &bootstrap_ip, uint16_t bootstrap_port);

    // 单次循环处理epoll事件
    void epoll_step();

    // 运行循环
    void run();

    // 优雅关闭，迁移数据并广播下线
    void gracefulShutdown();

    // 保存快照循环
    void snapshotLoop();

    KVStore<resp::RespValue> &getKVStore() { return kvstore; }
    AOF &getAOF() { return aof; }
    Cluster &getCluster() { return cluster; }
    std::mutex &getQueueMutex() { return queueMutex; }
    std::queue<std::pair<std::string, int>> &getMessageQueue() { return message_queue; }

    void requestShutdown() { shutdown_requested = true; }

private:
    // 存储引擎
    KVStore<resp::RespValue> kvstore;
    // 管理AOF的类
    AOF aof;
    // 是否正在运行
    volatile bool running = false;
    // 是否请求关闭
    volatile bool shutdown_requested = false;
    // 快照线程
    std::thread snapshot_thread;
    // 消息队列锁
    std::mutex queueMutex;
    // 消息队列
    std::queue<std::pair<std::string, int>> message_queue;
    // 主线程的socket连接
    std::map<int, Socket> connections;
    // 监听socket
    Socket server_sock;
    // epoll标识符
    int epoll_fd;
    // eventfd用于唤醒epoll
    int eventfd_fd = -1;
    // epoll事件
    epoll_event events[Config::MAX_EVENTS];
    // 线程池
    ThreadPool threadPool;
    // 集群类
    Cluster cluster;
};