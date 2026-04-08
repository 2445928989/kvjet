// Client.cpp
#include "Client.h"
#include "../resp/RespEncoder.h"
#include <chrono>
#include <climits>
#include <iostream>
#include <random>
#include <sstream>
#include <algorithm>
#include <iomanip>

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
        if (reqv.size() < 2) {
            return resp::RespValue(resp::Error("Usage: MGET key1 key2..."));
        }
        resp::Array ret;
        ret.value = std::vector<std::unique_ptr<resp::RespValue>>();
        for (auto &str : reqv) {
            ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(str))));
        }
        return ret;
    } else if (reqv[0] == "EXSITS") {
        if (reqv.size() < 2) {
            return resp::RespValue(resp::Error("Usage: EXISTS key1 key2..."));
        }
        resp::Array ret;
        ret.value = std::vector<std::unique_ptr<resp::RespValue>>();
        for (auto &str : reqv) {
            ret.value.value().push_back(std::make_unique<resp::RespValue>(resp::SimpleString(std::move(str))));
        }
        return ret;
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
        std::string encoded = resp::encode(request);
        
        // 记录发送时间
        auto send_time = std::chrono::high_resolution_clock::now();
        send(encoded);
        
        // 等待响应
        sock.parser().reset();
        while (!sock.parser().hasResult()) {
            char buf[4096];
            ssize_t n = ::recv(sock.fd(), buf, sizeof(buf), 0);
            if (n > 0) {
                sock.parser().append(std::string(buf, n));
            }
        }
        
        auto recv_time = std::chrono::high_resolution_clock::now();
        sock.parser().pop();  // 移除已处理的结果
        
        // 计算延迟（微秒）
        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(recv_time - send_time).count();
        latencies.push_back(latency_us / 1000.0);  // 转换为毫秒
        
        if ((i + 1) % (ops / 10) == 0 || i == 0) {
            std::cout << "Progress: " << (i + 1) << "/" << ops << std::endl;
        }
    }
    
    // 排序以计算百分位数
    std::sort(latencies.begin(), latencies.end());
    
    // 计算百分位数
    auto percentile = [&](double p) -> double {
        int idx = (int)((p / 100.0) * latencies.size());
        if (idx >= latencies.size()) idx = latencies.size() - 1;
        return latencies[idx];
    };
    
    double min_lat = latencies.front();
    double max_lat = latencies.back();
    double sum = 0;
    for (double lat : latencies) sum += lat;
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