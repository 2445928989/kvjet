// Server.cpp
#include "Server.h"
#include "Handler.h"
#include <climits>
#include <iostream>
#include <sys/fcntl.h>
Server::Server(uint16_t port) : server_sock(), threadPool(Config::THREAD_COUNT), kvstore(), aof(Config::AOF_DIR) {
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
    kvstore.readfromfile(Config::SNAPSHOT_DIR);
    aof.recover(&kvstore);
    std::cout << "Server Started" << std::endl;
}
Server::~Server() {
    if (epoll_fd != -1) {
        ::close(epoll_fd);
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
            if (ret.size() > Config::MAX_RECV_SIZE) {
                throw std::runtime_error("Command too large");
            }
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
    std::string message = Handler::handle(std::move(value), kvstore, aof);
    std::unique_lock<std::mutex> lock(queueMutex);
    message_queue.emplace(std::move(message), sock);
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
void Server::epoll_step() {
    int event_num = epoll_wait(epoll_fd, events, Config::MAX_EVENTS, 0);
    if (event_num == -1) {
        throw std::runtime_error("Epoll wait error");
    }
    for (size_t i = 0; i < (size_t)event_num; i++) {
        if (events[i].data.fd == server_sock.fd()) {
            while (accept()) {
            }
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
    snapshot_thread = std::thread([this] { snapshotLoop(); });
    while (running) {
        epoll_step();
    }
}

void Server::snapshotLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(Config::SNAPSHOT_INTERVAL_MS));
        std::filesystem::resize_file(Config::AOF_DIR, 0);
        kvstore.writetofile(Config::SNAPSHOT_DIR);
    }
}