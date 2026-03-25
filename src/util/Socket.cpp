// Socket.cpp
#include "Socket.h"

Socket::Socket() : Socket(AF_INET, SOCK_STREAM, 0) {}

Socket::Socket(int domain, int type, int protocol) {
    fd_ = ::socket(domain, type, protocol);
    if (fd_ == -1) {
        throw std::runtime_error("Socket error: " + std::string(strerror(errno)));
    }
}

Socket::~Socket() noexcept {
    if (fd_ != -1)
        close(fd_);
}

Socket::Socket(Socket &&other) noexcept {
    fd_ = other.fd_;
    other.fd_ = -1;
}

Socket &Socket::operator=(Socket &&other) noexcept {
    if (this != &other) {
        if (fd_ != -1)
            close(fd_);
        this->fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

int Socket::fd() const {
    return fd_;
}

void Socket::bind(const std::string &ip, uint16_t port) {
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid IP address: " + ip);
    }
    if (::bind(fd_, (sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        throw std::runtime_error("Bind error: " + std::string(strerror(errno)));
    }
}

void Socket::connect(const std::string &ip, uint16_t port) {
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    addrinfo *result = nullptr;
    const std::string service = std::to_string(port);
    const int gai_ret = ::getaddrinfo(ip.c_str(), service.c_str(), &hints, &result);
    if (gai_ret != 0) {
        throw std::runtime_error("Gethost error: " + std::string(::gai_strerror(gai_ret)));
    }
    if (result == nullptr) {
        throw std::runtime_error("Gethost error: no address found");
    }

    if (::connect(fd_, result->ai_addr, result->ai_addrlen) == -1) {
        const int connect_errno = errno;
        ::freeaddrinfo(result);
        throw std::runtime_error("Connect error: " + std::string(strerror(connect_errno)));
    }

    ::freeaddrinfo(result);
}
void Socket::listen(int backlog) {
    if (::listen(fd_, backlog) == -1) {
        throw std::runtime_error("Listen error: " + std::string(strerror(errno)));
    }
}

Socket Socket::accept() {
    int client_fd = ::accept(fd_, nullptr, nullptr);
    if (client_fd == -1) {
        throw std::runtime_error("Accept error: " + std::string(strerror(errno)));
    }
    return Socket(client_fd);
}

Socket::Socket(int fd) noexcept : fd_(fd) {}