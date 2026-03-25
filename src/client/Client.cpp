// Client.cpp
#include "Client.h"
#include <climits>
#include <iostream>

Client::Client(const std::string &ip, uint16_t port) : sock() {
    sock.connect(ip, port);
}

Client::~Client() {}

ssize_t Client::send(const std::string &request) {
    if (request.size() > static_cast<size_t>(SSIZE_MAX)) {
        throw std::runtime_error("Request too long");
    }
    size_t remaining = request.size();
    size_t sent = 0;
    const char *data = request.c_str();
    while (remaining != 0) {
        ssize_t n = ::send(sock.fd(), data + sent, remaining, MSG_NOSIGNAL);
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
std::string Client::recv() {
    char buf[1024];
    ssize_t n = ::recv(sock.fd(), buf, sizeof(buf), 0);
    if (n > 0) {
        return std::string(buf, n);
    } else if (n == 0) {
        throw std::runtime_error("Connection closed");
    } else {
        throw std::runtime_error("Recv error: " + std::string(strerror(errno)));
    }
}

void Client::run() {
    std::string request;
    while (std::cin >> request) {
        send(request);
        std::cout << recv() << '\n';
    }
}