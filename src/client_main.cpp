// client_main.cpp
#include "client/Client.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <ip> <port> [--benchmark <ops> <type> [--threads <n>]]\n";
        std::cout << "  type: set, get, mixed\n";
        std::cout << "  threads: number of concurrent connections (default: 1)\n";
        return 1;
    }

    std::string ip = argv[1];
    uint16_t port = atoi(argv[2]);

    bool enable_benchmark = false;
    int benchmark_ops = 10000;
    std::string benchmark_type = "set";
    int num_threads = 1;

    if (argc >= 5 && std::string(argv[3]) == "--benchmark") {
        enable_benchmark = true;
        benchmark_ops = atoi(argv[4]);
        if (argc >= 6) {
            benchmark_type = argv[5];
        }
        if (argc >= 8 && std::string(argv[6]) == "--threads") {
            num_threads = atoi(argv[7]);
        }
    }

    try {
        if (enable_benchmark && num_threads > 1) {
            std::cout << "Starting benchmark with " << num_threads << " threads..." << std::endl;
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

            std::cout << "\n=== Total Results ===" << std::endl;
            std::cout << "Total operations: " << total_ops << std::endl;
            std::cout << "Total time: " << elapsed << "ms" << std::endl;
            std::cout << "Total QPS: " << total_qps << std::endl;

        } else {
            Client client(ip, port);
            if (enable_benchmark) {
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