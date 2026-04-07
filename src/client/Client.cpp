// Client.cpp
#include "Client.h"
#include "../resp/RespEncoder.h"
#include <chrono>
#include <climits>
#include <iostream>
#include <random>
#include <sstream>

Client::Client(const std::string &ip, uint16_t port) : sock() {
    sock.connect(ip, port);
    std::cout << "Connected." << std::endl;
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
    std::cout << ">>> ";
    while (std::getline(std::cin, request)) {
        if (!request.empty()) {
            resp::RespValue req = handle(std::move(request));
            if (auto it = std::get_if<resp::Error>(req.getPtr())) {
                std::cout << it->value << '\n';
                if (it->value == "GoodBye.") {
                    return;
                }
            } else {
                send(std::move(resp::encode(req)));
                std::cout << recv() << '\n';
            }
        }
        std::cout << ">>> ";
    }
}

resp::RespValue Client::handle(std::string req) {
    std::vector<std::string> reqv;
    std::stringstream ss(req);
    std::string tmp;
    while (ss >> tmp) {
        reqv.push_back(std::move(tmp));
    }
    if (reqv[0] == "GET") {
        if (reqv.size() != 2) {
            return resp::RespValue(resp::Error("Usage: GET key"));
        }
        resp::Array ret;
        ret.value = std::vector<std::unique_ptr<resp::RespValue>>();
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(reqv[0]))));
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(reqv[1]))));
        return ret;
    } else if (reqv[0] == "SET") {
        if (reqv.size() != 3) {
            return resp::RespValue(resp::Error("Usage: SET key value"));
        }
        resp::Array ret;
        ret.value = std::vector<std::unique_ptr<resp::RespValue>>();
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(reqv[0]))));
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(reqv[1]))));
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::BulkString(std::move(reqv[2]))));
        return ret;
    } else if (reqv[0] == "MGET") {

    } else if (reqv[0] == "EXSITS") {

    } else if (reqv[0] == "HELP") {
        std::string help = "Commands:\n"
                           "    SET key value        Set a key-value pair\n"
                           "    GET key              Get value by key\n"
                           "    DEL key [key ...]    Delete one or more keys\n"
                           "    EXISTS key [key ...] Check if keys exist\n"
                           "    MGET key [key ...]   Get multiple values\n"
                           "    HELP                 Show this message\n"
                           "    EXIT                 Disconnect and exit\n\n"
                           "Examples:\n"
                           "    SET mykey hello\n"
                           "    GET mykey\n"
                           "    DEL mykey\n"
                           "    MGET key1 key2 key3";
        return resp::RespValue(resp::Error(std::move(help)));
    } else if (reqv[0] == "EXIT") {
        return resp::RespValue(resp::Error("GoodBye."));
    } else {
        return resp::RespValue(resp::Error("Unknown command. Type HELP for assistance."));
    }
    return resp::RespValue(resp::Error("Unknown Error"));
}
std::mt19937 rnd(time(0));
void Client::benchmark(int ops, const std::string &op_type) {
    std::cout << "Start benchmark..." << std::endl;

    int batch_size = 5000;
    std::vector<std::string> requests;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ops; i++) {
        std::string req;
        if (op_type == "set") {
            req = "SET key" + std::to_string(rnd() % 100000) + " value" + std::to_string(rnd() % 100000);
        } else if (op_type == "get") {
            req = "GET key" + std::to_string(rnd() % 100000);
        } else if (op_type == "mixed") {
            if (i % 2) {
                req = "SET key" + std::to_string(rnd() % 100000) + " value" + std::to_string(rnd() % 100000);
            } else {
                req = "GET key" + std::to_string(rnd() % 100000);
            }
        }

        resp::RespValue request = handle(std::move(req));
        requests.push_back(resp::encode(request));

        if (requests.size() >= batch_size || i == ops - 1) {
            for (const auto &r : requests) {
                send(r);
            }
            int remaining = requests.size();
            while (remaining > 0) {
                char buf[4096];
                ssize_t n = ::recv(sock.fd(), buf, sizeof(buf), 0);
                if (n > 0) {
                    sock.parser().append(std::string(buf, n));
                    while (sock.parser().hasResult()) {
                        sock.parser().pop();
                        remaining--;
                    }
                }
            }
            requests.clear();
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    double qps = (ops * 1000.0) / elapsed;

    std::cout << "QPS: " << qps << std::endl;
}