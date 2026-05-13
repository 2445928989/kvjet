// Server.cpp
#include "Server.h"
#include "../util/Utils.h"
#include "Handler.h"
#include <climits>
#include <csignal>
#include <iostream>
#include <sys/eventfd.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
Server::Server(const std::string &ip, uint16_t port) : server_sock(), threadPool(Config::THREAD_COUNT), kvstore(), aof(Config::AOF_DIR), cluster(port + 1, ip) {
    memset(events, 0, sizeof(events));
    server_sock.bind("0.0.0.0", port);
    server_sock.listen();
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        throw std::runtime_error("Epoll create error: " + std::string(strerror(errno)));
    }
    int flags = fcntl(server_sock.fd(), F_SETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl(F_GETFL) error: " + std::string(strerror(errno)));
    }
    if (fcntl(server_sock.fd(), F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl(F_SETFL) error: " + std::string(strerror(errno)));
    }
    epoll_event event{};
    event.data.fd = server_sock.fd();
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock.fd(), &event) == -1) {
        throw std::runtime_error("Epoll add error: " + std::string(strerror(errno)));
    }

    // 初始化 eventfd 用于唤醒 epoll
    eventfd_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (eventfd_fd == -1) {
        throw std::runtime_error("Eventfd error: " + std::string(strerror(errno)));
    }
    epoll_event event_fd_event{};
    event_fd_event.data.fd = eventfd_fd;
    event_fd_event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, eventfd_fd, &event_fd_event) == -1) {
        throw std::runtime_error("Epoll add eventfd error: " + std::string(strerror(errno)));
    }

    // 设置心跳发送回调：推入消息队列，由主线程统一发送
    cluster.setSendCallback([this](const std::string &msg, int fd) {
        std::unique_lock<std::mutex> lock(queueMutex);
        message_queue.emplace(msg, fd);
    });

    kvstore.readfromfile(Config::SNAPSHOT_DIR);
    aof.recover(*this);
    std::cout << "Server Started" << std::endl;
}
Server::~Server() {
    if (epoll_fd != -1) {
        ::close(epoll_fd);
    }
    if (eventfd_fd != -1) {
        ::close(eventfd_fd);
    }
    running = false;
    if (snapshot_thread.joinable()) {
        snapshot_thread.join();
    }
}

ssize_t Server::send(const std::string &str, const Socket &sock) {
    if (str.size() > static_cast<size_t>(SSIZE_MAX)) {
        throw std::runtime_error("Data too long");
    }
    size_t remaining = str.size();
    size_t sent = 0;
    const char *data = str.c_str();
    while (remaining != 0) {
        ssize_t n = ::send(sock.fd(), data + sent, remaining, MSG_NOSIGNAL);
        if (n == 0) {
            // TODO: 记录客户端已经断开无法send
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock.fd(), nullptr);
            connections.erase(sock.fd());
            return static_cast<ssize_t>(sent);
        } else if (n == -1) {
            if (errno == EINTR)
                continue;
            if (errno == EBADF)
                break;
            throw std::runtime_error("Send error: " + std::string(strerror(errno)));
        }
        sent += static_cast<size_t>(n);
        remaining -= static_cast<size_t>(n);
    }
    return static_cast<ssize_t>(sent);
}

std::string Server::recv(const Socket &sock) {
    char buf[Config::MAX_RECV_SIZE];
    std::string ret;
    ssize_t n;
    while (true) {
        n = ::recv(sock.fd(), buf, sizeof(buf), 0);
        if (n > 0) {
            ret += std::string(buf, n);
        } else if (n == 0) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock.fd(), nullptr);
            connections.erase(sock.fd());
            return ret;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock.fd(), nullptr) == -1) {
                    throw std::runtime_error("Epoll delete error: " + std::string(strerror(errno)));
                }
                connections.erase(sock.fd());
            }
            return ret;
        }
    }
}
void Server::handleCommand(int sock, resp::RespValue value) {
    std::string message = Handler::handle(std::move(value), *this, sock);
    if (message.empty())
        return;
    std::unique_lock<std::mutex> lock(queueMutex);
    message_queue.emplace(std::move(message), sock);
    lock.unlock();

    // 唤醒 epoll
    uint64_t one = 1;
    ::write(eventfd_fd, &one, sizeof(one));
}
bool Server::accept() {
    Socket sock = server_sock.accept();
    if (sock.fd() == -1)
        return false;
    int fd = sock.fd();
    int flags = fcntl(fd, F_SETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl(F_GETFL) error: " + std::string(strerror(errno)));
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl(F_SETFL) error: " + std::string(strerror(errno)));
    }
    epoll_event event{};
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw std::runtime_error("Epoll add error: " + std::string(strerror(errno)));
    }
    connections[fd] = std::move(sock);
    return true;
}
void Server::connect(const std::string &ip, uint16_t port, uint64_t uuid) {
    Socket sock;
    sock.connect(ip, port);
    int fd = sock.fd();
    int flags = fcntl(fd, F_SETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl(F_GETFL) error: " + std::string(strerror(errno)));
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl(F_SETFL) error: " + std::string(strerror(errno)));
    }
    epoll_event event{};
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw std::runtime_error("Epoll add error: " + std::string(strerror(errno)));
    }
    send("*2\r\n+HELLO\r\n+" + std::to_string(cluster.getSelf().UUID) + "\r\n", sock);
    while (!sock.parser().hasResult()) {
        sock.parser().append(recv(sock));
    }
    // 里边是个+OK
    sock.parser().getResult();
    connections[fd] = std::move(sock);
    cluster.addConnection(uuid, fd);
    threadPool.enqueue([this, uuid]() {
        sendDataToNode(uuid);
    });
}

void Server::sendDataToNode(uint64_t target_uuid) {
    int target_fd = cluster.getConnection(target_uuid);
    if (target_fd == -1) {
        std::cerr << "Failed to get fd for UUID: " << target_uuid << std::endl;
        return;
    }
    kvstore.forEach([this, target_uuid, target_fd](const std::string &key, resp::RespValue *value) {
        uint64_t owner_uuid = cluster.queryNode(key);
        if (owner_uuid == target_uuid) {
            std::string cmd = "*3\r\n+SET\r\n+" + key + "\r\n" + resp::encode(*value);
            std::unique_lock<std::mutex> lock(queueMutex);
            message_queue.emplace(std::move(cmd), target_fd);
        }
    });

    // 通知目标节点：本轮数据迁移结束
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        message_queue.emplace(
            "*2\r\n+SYNCDONE\r\n+" + std::to_string(cluster.getSelf().UUID) + "\r\n",
            target_fd);
    }
}
void Server::epoll_step() {
    int event_num = epoll_wait(epoll_fd, events, Config::MAX_EVENTS, 1);
    if (event_num == -1) {
        if (errno == EINTR)
            return; // 被信号中断，继续循环
        throw std::runtime_error("Epoll wait error: " + std::string(strerror(errno)));
    }
    for (size_t i = 0; i < (size_t)event_num; i++) {
        if (events[i].data.fd == server_sock.fd()) {
            while (accept()) {
            }
        } else if (events[i].data.fd == eventfd_fd) {
            // eventfd 被唤醒，清空计数器
            uint64_t val;
            ::read(eventfd_fd, &val, sizeof(val));
        } else {
            int client_fd = events[i].data.fd;
            std::string str = recv(connections[client_fd]);
            if (str.empty()) {
                continue;
            }
            auto it = connections.find(client_fd);
            if (it == connections.end()) {
                // TODO: 引入日志或者warning来记录：
                // 虽然读完了输入，但是客户端已经下线
                continue;
            }
            it->second.parser().append(std::move(str));
            while (it->second.parser().hasResult()) {
                auto value = std::move(it->second.parser().getResult().value());
                int sock = it->second.fd();
                auto value_ptr = std::make_unique<resp::RespValue>(std::move(value));
                threadPool.enqueue([this, sock, v = std::move(value_ptr)]() {
                    handleCommand(sock, std::move(*v));
                });
            }
        }
    }
    std::unique_lock<std::mutex> lock(queueMutex);
    while (!message_queue.empty()) {
        auto message = std::move(message_queue.front());
        message_queue.pop();
        if (message.second == -1) {
            std::istringstream iss(message.first);
            std::string ip;
            uint16_t port;
            uint64_t uuid;
            iss >> ip >> port >> uuid;
            connect(ip, port, uuid);
            continue;
        }
        auto it = connections.find(message.second);

        if (it != connections.end())
            send(std::move(message.first), it->second);
        else {
            // TODO: 引入日志或者warning系统来记录这种情况：
            // 任务处理完了，但是客户端已经下线了
            continue;
        }
    }
}
void Server::run() {
    running = true;
    cluster.start();
    snapshot_thread = std::thread([this] { snapshotLoop(); });
    while (running && !shutdown_requested) {
        epoll_step();
    }
    gracefulShutdown();
}

void Server::gracefulShutdown() {
    std::cout << "\nStarting graceful shutdown..." << std::endl;

    running = false; // 停止接收新请求

    uint64_t gossip_uuid = cluster.generateUUID();
    cluster.addGossip(gossip_uuid);

    std::string nodeout_cmd = "*3\r\n+NODEOUT\r\n+" + std::to_string(gossip_uuid) +
                              "\r\n+" + std::to_string(cluster.getSelf().UUID) + "\r\n";

    auto connections = cluster.getConnections();
    uint64_t self_uuid = cluster.getSelf().UUID;
    for (auto &[uuid, fd] : connections) {
        if (uuid != self_uuid) {
            ::send(fd, nodeout_cmd.c_str(), nodeout_cmd.size(), MSG_NOSIGNAL);
        }
    }
    cluster.delNodeToHash(cluster.getSelf().UUID);
    cluster.delTopoNode(cluster.getSelf().UUID);
    // 遍历kvstore，把所有数据转发给新owner
    kvstore.forEach([this](const std::string &key, resp::RespValue *value) {
        uint64_t owner_uuid = cluster.queryNode(key);
        if (owner_uuid != cluster.getSelf().UUID) {
            int target_fd = cluster.getConnection(owner_uuid);
            if (target_fd != -1) {
                std::string set_cmd = "*3\r\n+SET\r\n+" + key + "\r\n" + resp::encode(*value);
                ::send(target_fd, set_cmd.c_str(), set_cmd.size(), MSG_NOSIGNAL);
            }
        }
    });

    std::cout << "Graceful shutdown complete." << std::endl;
}

void Server::snapshotLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(Config::SNAPSHOT_INTERVAL_MS));
        std::filesystem::resize_file(Config::AOF_DIR, 0);
        kvstore.writetofile(Config::SNAPSHOT_DIR);
    }
}

void Server::joinCluster(const std::string &bootstrap_ip, uint16_t bootstrap_port) {
    std::cout << "Try to join Cluster..." << std::endl;
    Socket sock;
    sock.connect(bootstrap_ip, bootstrap_port);
    // 设置socket为非阻塞模式
    int fd = sock.fd();
    int flags = fcntl(fd, F_SETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl(F_GETFL) error: " + std::string(strerror(errno)));
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl(F_SETFL) error: " + std::string(strerror(errno)));
    }

    if (send("*1\r\n+GETNETWORK\r\n", sock) == -1) {
        throw std::runtime_error("GETNETWORK failed");
    }
    while (!sock.parser().hasResult()) {
        std::string str = recv(sock);
        sock.parser().append(str);
    }
    auto topo = Utils::getTopo(sock.parser().getResult().value());

    // 进入同步状态：所有现有节点将向我推送属于我的数据
    markSyncing(static_cast<int>(topo.size()));

    for (auto &node : topo) {
        getCluster().addTopoNode(node);
        getCluster().addNodeToHash(node.UUID);
        if (node.ip == bootstrap_ip && node.port == bootstrap_port) {
            getCluster().addConnection(node.UUID, sock.fd());
        }
    }

    if (send("*2\r\n+HELLO\r\n+" + std::to_string(getCluster().getSelf().UUID) + "\r\n", sock) == -1) {
        throw std::runtime_error("SEND HELLO failed");
    }

    while (!sock.parser().hasResult()) {
        sock.parser().append(recv(sock));
    }
    // 里边是个+OK
    sock.parser().reset();
    uint64_t msg_UUID = getCluster().generateUUID();
    getCluster().addGossip(msg_UUID);
    if (send("*3\r\n+NODEIN\r\n+" + std::to_string(msg_UUID) + "\r\n" + resp::encodeNode(getCluster().getSelf()), sock) == -1) {
        throw std::runtime_error("gossip:NODEIN failed");
    }
    // 将socket添加到epoll
    epoll_event event{};

    event.data.fd = fd;

    event.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw std::runtime_error("Epoll add error: " + std::string(strerror(errno)));
    }

    connections[fd] = std::move(sock);

    std::cout << "Joined to Cluster" << std::endl;
}
