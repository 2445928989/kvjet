// Client.cpp (Fixed version)
#include "Client.h"
#include "../resp/RespEncoder.h"
#include "../util/Utils.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

Client::Client(const std::string &ip, uint16_t port) : cluster() {
    Socket sock;
    sock.connect(ip, port);
    std::cout << "Connected to " << ip << ":" << port << std::endl;

    std::string getnetwork_cmd = "*1\r\n+GETNETWORK\r\n";
    size_t sent = 0;
    while (sent < getnetwork_cmd.size()) {
        ssize_t n = ::send(sock.fd(), getnetwork_cmd.c_str() + sent, getnetwork_cmd.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
            throw std::runtime_error("Failed to send GETNETWORK");
        sent += n;
    }

    while (!sock.parser().hasResult()) {
        char buf[1024];
        ssize_t n = ::recv(sock.fd(), buf, sizeof(buf), 0);
        if (n <= 0)
            throw std::runtime_error("Failed to recv GETNETWORK");
        sock.parser().append(std::string(buf, n));
    }

    auto topo_vec = Utils::getTopo(sock.parser().getResult().value());
    sock.parser().reset();

    // 加载拓扑到cluster
    for (auto &node : topo_vec) {
        cluster.addTopoNode(node);
        cluster.addNodeToHash(node.UUID);
    }

    // 连接到所有节点
    for (auto &node : topo_vec) {
        if (node.ip == ip && node.port == port) {
            // 初始连接
            cluster.addConnection(node.UUID, sock.fd());
            connections[sock.fd()] = std::move(sock);
            continue;
        }
        Socket other_sock;
        other_sock.connect(node.ip, node.port);
        int fd = other_sock.fd();
        cluster.addConnection(node.UUID, fd);
        connections[fd] = std::move(other_sock);
        std::cout << "Connected to " << node.ip << ":" << node.port << " (UUID: " << node.UUID << ")" << std::endl;
    }
}

Client::~Client() {}

Socket &Client::getSocket(int fd) {
    auto it = connections.find(fd);
    if (it == connections.end()) {
        throw std::runtime_error("Socket not found for fd: " + std::to_string(fd));
    }
    return it->second;
}

ssize_t Client::send(const std::string &request, const std::string &key) {
    uint64_t target_uuid = cluster.queryNode(key);
    int target_fd = cluster.getConnection(target_uuid);
    if (target_fd == -1) {
        throw std::runtime_error("Failed to get connection for node UUID: " + std::to_string(target_uuid));
    }

    Socket &target_sock = getSocket(target_fd);
    if (request.size() > static_cast<size_t>(SSIZE_MAX)) {
        throw std::runtime_error("Request too long");
    }
    size_t remaining = request.size();
    size_t sent = 0;
    const char *data = request.c_str();
    while (remaining != 0) {
        ssize_t n = ::send(target_sock.fd(), data + sent, remaining, MSG_NOSIGNAL);
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

std::string Client::recv(const std::string &key) {
    uint64_t target_uuid = cluster.queryNode(key);
    int target_fd = cluster.getConnection(target_uuid);
    if (target_fd == -1) {
        throw std::runtime_error("Failed to get connection for node UUID: " + std::to_string(target_uuid));
    }

    Socket &target_sock = getSocket(target_fd);

    char buf[1024];
    ssize_t n = ::recv(target_sock.fd(), buf, sizeof(buf), 0);
    if (n > 0) {
        return std::string(buf, n);
    } else if (n == 0) {
        throw std::runtime_error("Connection closed");
    } else {
        throw std::runtime_error("Recv error: " + std::string(strerror(errno)));
    }
}

void Client::run() {
    std::string input;
    std::cout << ">>> ";
    while (std::getline(std::cin, input)) {
        if (!input.empty()) {
            resp::RespValue req = handle(std::move(input));
            if (auto it = std::get_if<resp::Error>(req.getPtr())) {
                std::cout << it->value << '\n';
                if (it->value == "GoodBye.") {
                    return;
                }
            } else {
                std::string key = extractKeyFromRequest(req);
                send(resp::encode(req), key);
                std::cout << recv(key) << '\n';
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
    } else if (reqv[0] == "DEL") {
        if (reqv.size() != 2) {
            return resp::RespValue(resp::Error("Usage: DEL key"));
        }
        resp::Array ret;
        ret.value = std::vector<std::unique_ptr<resp::RespValue>>();
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(reqv[0]))));
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(reqv[1]))));
        return ret;
    } else if (reqv[0] == "EXIST") {
        if (reqv.size() != 2) {
            return resp::RespValue(resp::Error("Usage: EXIST key"));
        }
        resp::Array ret;
        ret.value = std::vector<std::unique_ptr<resp::RespValue>>();
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(reqv[0]))));
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(reqv[1]))));
        return ret;
    } else if (reqv[0] == "HELP") {
        std::string help = "Commands:\n"
                           "    SET key value        Set a key-value pair\n"
                           "    GET key              Get value by key\n"
                           "    DEL key              Delete a key\n"
                           "    EXIST key           Check if key exists\n"
                           "    HELP                 Show this message\n"
                           "    EXIT                 Disconnect and exit\n\n"
                           "Examples:\n"
                           "    SET mykey hello\n"
                           "    GET mykey\n"
                           "    DEL mykey\n"
                           "    EXIST mykey";
        return resp::RespValue(resp::Error(std::move(help)));
    } else if (reqv[0] == "EXIT") {
        return resp::RespValue(resp::Error("GoodBye."));
    } else if (reqv[0] == "GETNETWORK") {
        resp::Array ret;
        ret.value = std::vector<std::unique_ptr<resp::RespValue>>();
        ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(reqv[0]))));
        return ret;
    } else {
        return resp::RespValue(resp::Error("Unknown command. Type HELP for assistance."));
    }
    return resp::RespValue(resp::Error("Unknown Error"));
}

std::string Client::extractKeyFromRequest(const resp::RespValue &req) {
    auto arr = std::get_if<resp::Array>(req.getPtr());
    if (arr->value->size() < 2) {
        return "";
    }
    if (!arr || !arr->value) {
        throw std::runtime_error("Invalid request format");
    }
    auto key_ptr = arr->value.value()[1].get();
    if (auto key_str = std::get_if<resp::SimpleString>(key_ptr->getPtr())) {
        return key_str->value;
    } else {
        throw std::runtime_error("Key is not a SimpleString");
    }
}

std::mt19937 rnd(time(0));

void Client::benchmark(int ops, const std::string &op_type) {
    std::cout << "Start benchmark..." << std::endl;

    int batch_size = 5000;
    std::map<uint64_t, std::vector<std::pair<std::string, std::string>>> pending; // uuid -> [(encoded, key), ...]

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
        std::string key = extractKeyFromRequest(request);
        uint64_t target_uuid = cluster.queryNode(key);
        std::string encoded = resp::encode(request);

        pending[target_uuid].push_back({encoded, key});

        // 如果某个节点的batch达到batch_size，立即发送并接收
        if (pending[target_uuid].size() >= batch_size) {
            // 发送全部
            for (const auto &[enc, k] : pending[target_uuid]) {
                send(enc, k);
            }
            // 接收全部
            int remaining = pending[target_uuid].size();
            int target_fd = cluster.getConnection(target_uuid);
            Socket &target_sock = getSocket(target_fd);

            while (remaining > 0) {
                char buf[4096];
                ssize_t n = ::recv(target_sock.fd(), buf, sizeof(buf), 0);
                if (n > 0) {
                    target_sock.parser().append(std::string(buf, n));
                    while (target_sock.parser().hasResult()) {
                        target_sock.parser().pop();
                        remaining--;
                    }
                }
            }
            pending[target_uuid].clear();
        }
    }

    // 处理剩余的pending操作
    for (auto &[uuid, ops_list] : pending) {
        // 发送全部
        for (const auto &[encoded, key] : ops_list) {
            send(encoded, key);
        }
        // 接收全部
        int remaining = ops_list.size();
        int target_fd = cluster.getConnection(uuid);
        Socket &target_sock = getSocket(target_fd);

        while (remaining > 0) {
            char buf[4096];
            ssize_t n = ::recv(target_sock.fd(), buf, sizeof(buf), 0);
            if (n > 0) {
                target_sock.parser().append(std::string(buf, n));
                while (target_sock.parser().hasResult()) {
                    target_sock.parser().pop();
                    remaining--;
                }
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    double qps = (ops * 1000.0) / elapsed;

    std::cout << "QPS: " << qps << std::endl;
}

void Client::latencyBenchmark(int ops, const std::string &op_type) {
    std::cout << "Starting latency benchmark (" << ops << " operations)..." << std::endl;

    std::vector<double> latencies;
    latencies.reserve(ops);

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
        std::string key = extractKeyFromRequest(request);
        std::string encoded = resp::encode(request);

        // 记录发送时间
        auto send_time = std::chrono::high_resolution_clock::now();
        send(encoded, key);

        // 获取对应的socket和parser
        uint64_t target_uuid = cluster.queryNode(key);
        int target_fd = cluster.getConnection(target_uuid);
        Socket &target_sock = getSocket(target_fd);

        // 等待响应
        target_sock.parser().reset();
        while (!target_sock.parser().hasResult()) {
            char buf[4096];
            ssize_t n = ::recv(target_sock.fd(), buf, sizeof(buf), 0);
            if (n > 0) {
                target_sock.parser().append(std::string(buf, n));
            }
        }

        auto recv_time = std::chrono::high_resolution_clock::now();
        target_sock.parser().pop(); // 移除已处理的结果

        // 计算延迟（微秒）
        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(recv_time - send_time).count();
        latencies.push_back(latency_us / 1000.0); // 转换为毫秒

        if ((i + 1) % (ops / 10) == 0 || i == 0) {
            std::cout << "Progress: " << (i + 1) << "/" << ops << std::endl;
        }
    }

    // 排序以计算百分位数
    std::sort(latencies.begin(), latencies.end());

    // 计算百分位数
    auto percentile = [&](double p) -> double {
        int idx = (int)((p / 100.0) * latencies.size());
        if (idx >= latencies.size())
            idx = latencies.size() - 1;
        return latencies[idx];
    };

    double min_lat = latencies.front();
    double max_lat = latencies.back();
    double sum = 0;
    for (double lat : latencies)
        sum += lat;
    double avg_lat = sum / latencies.size();

    std::cout << "\n=== Latency Results ===" << std::endl;
    std::cout << "Total requests: " << ops << std::endl;
    std::cout << "Latency (ms):" << std::endl;
    std::cout << "  Min:    " << std::fixed << std::setprecision(2) << min_lat << std::endl;
    std::cout << "  P50:    " << std::fixed << std::setprecision(2) << percentile(50) << std::endl;
    std::cout << "  P90:    " << std::fixed << std::setprecision(2) << percentile(90) << std::endl;
    std::cout << "  P99:    " << std::fixed << std::setprecision(2) << percentile(99) << std::endl;
    std::cout << "  P99.9:  " << std::fixed << std::setprecision(2) << percentile(99.9) << std::endl;
    std::cout << "  Max:    " << std::fixed << std::setprecision(2) << max_lat << std::endl;
    std::cout << "  Avg:    " << std::fixed << std::setprecision(2) << avg_lat << std::endl;
}
