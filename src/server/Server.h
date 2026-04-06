// Server.h
#pragma once
#include "../resp/RespValue.h"
#include "../util/AOF.h"
#include "../util/KVStore.h"
#include "../util/Socket.h"
#include "../util/ThreadPool.h"
#include <cerrno>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <sys/epoll.h>
// 服务端类
class Server {
// epoll 单轮事件数
#define MAX_EVENTS 1024
// recv() 最长字符串长度
#define MAX_RECV_SIZE 1024 * 1024
public:
    // 构造函数，传入一个端口
    Server(uint16_t port);
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

    // 单次循环处理epoll事件
    void epoll_step();

    // 运行循环
    void run();

private:
    std::mutex queueMutex;
    std::queue<std::pair<std::string, int>> message_queue;
    std::map<int, Socket> connections;
    Socket server_sock;
    int epoll_fd;
    epoll_event events[MAX_EVENTS];
    KVStore<resp::RespValue> kvstore;
    ThreadPool threadPool;
    AOF aof;
};