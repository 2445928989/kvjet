// Server.cpp
#include "Server.h"
#include <climits>

Server::Server(uint16_t port) : server_sock() {
    server_sock.bind("0.0.0.0", port);
}
Server::~Server() {}
ssize_t Server::send(const std::string &str) {
    if (!client_sock.has_value()) {
        throw std::runtime_error("No client connected");
    }
    if (str.size() > static_cast<size_t>(SSIZE_MAX)) {
        throw std::runtime_error("Data too long");
    }
    size_t remaining = str.size();
    size_t sent = 0;
    const char *data = str.c_str();
    while (remaining != 0) {
        ssize_t n = ::send(client_sock->fd(), data + sent, remaining, MSG_NOSIGNAL);
        if (n == 0) {
            throw std::runtime_error("Connection closed");
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

std::string Server::recv() {
    if (!client_sock.has_value()) {
        throw std::runtime_error("No client connected");
    }
    char buf[1024];
    ssize_t n = ::recv(client_sock->fd(), buf, sizeof(buf), 0);
    if (n > 0) {
        return std::string(buf, n);
    } else if (n == 0) {
        throw std::runtime_error("Connection closed");
    } else {
        throw std::runtime_error("Recv error: " + std::string(strerror(errno)));
    }
}

void Server::run() {
    client_sock = server_sock.accept();
    std::string buf;
    while (true) {
        buf = recv();
        send(buf);
    }
}