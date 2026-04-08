// client_main.cpp
#include "client/Client.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "KVJet Client v1.0\n\n";
        std::cout << "Usage: " << argv[0] << " <ip> <port> [COMMAND] [OPTIONS]\n\n";
        std::cout << "Commands:\n";
        std::cout << "  (no command)                    Interactive mode\n";
        std::cout << "  --benchmark <ops> <type>       Throughput test (with pipelining)\n";
        std::cout << "  --latency <ops> <type>         Latency test (P50/P99/P99.9)\n\n";
        std::cout << "Operation types: set, get, mixed\n\n";
        std::cout << "Options:\n";
        std::cout << "  --threads <n>                  Number of concurrent connections (default: 1)\n\n";
        return 1;
    }

    std::string ip = argv[1];
    uint16_t port = atoi(argv[2]);

    bool enable_benchmark = false;
    bool enable_latency = false;
    int benchmark_ops = 10000;
    std::string benchmark_type = "set";
    int num_threads = 1;

    // 参数解析
    if (argc >= 5) {
        std::string cmd = argv[3];
        if (cmd == "--benchmark") {
            enable_benchmark = true;
            benchmark_ops = atoi(argv[4]);
            if (argc >= 6) {
                benchmark_type = argv[5];
            }
        } else if (cmd == "--latency") {
            enable_latency = true;
            benchmark_ops = atoi(argv[4]);
            if (argc >= 6) {
                benchmark_type = argv[5];
            }
        }

        // 查找 --threads 参数
        for (int i = 3; i < argc - 1; i++) {
            if (std::string(argv[i]) == "--threads") {
                num_threads = atoi(argv[i + 1]);
                break;
            }
        }
    }

    try {
        // Benchmark多线程
        if (enable_benchmark && num_threads > 1) {
            std::cout << "Starting QPS benchmark with " << num_threads << " threads ("
                      << benchmark_ops << " ops each)...\n"
                      << std::endl;
            std::vector<std::thread> threads;
            auto start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&ip, port, benchmark_ops, &benchmark_type]() {
                    try {
                        Client client(ip, port);
                        client.benchmark(benchmark_ops, benchmark_type);
                    } catch (const std::exception &e) {
                        std::cerr << "Thread error: " << e.what() << '\n';
                    }
                });
            }

            for (auto &t : threads) {
                t.join();
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            int total_ops = benchmark_ops * num_threads;
            double total_qps = (total_ops * 1000.0) / elapsed;

            std::cout << "\n=== Benchmark Results ===" << std::endl;
            std::cout << "Total operations: " << total_ops << std::endl;
            std::cout << "Total time: " << elapsed << " ms" << std::endl;
            std::cout << "Total QPS: " << total_qps << std::endl;

        } else if (enable_latency && num_threads > 1) {
            std::cout << "Starting latency benchmark with " << num_threads << " threads ("
                      << benchmark_ops << " ops each)...\n"
                      << std::endl;
            std::vector<std::thread> threads;

            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([i, &ip, port, benchmark_ops, &benchmark_type]() {
                    try {
                        Client client(ip, port);
                        std::cout << "\n>> Thread " << i << " starting\n"
                                  << std::endl;
                        client.latencyBenchmark(benchmark_ops, benchmark_type);
                    } catch (const std::exception &e) {
                        std::cerr << "Thread " << i << " error: " << e.what() << '\n';
                    }
                });
            }

            for (auto &t : threads) {
                t.join();
            }

        } else {
            // 单线程模式
            Client client(ip, port);
            if (enable_latency) {
                client.latencyBenchmark(benchmark_ops, benchmark_type);
            } else if (enable_benchmark) {
                client.benchmark(benchmark_ops, benchmark_type);
            } else {
                client.run();
            }
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}