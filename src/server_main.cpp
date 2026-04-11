#include "config/Config.h"
#include "server/Server.h"
#include <csignal>
#include <cstdlib>
#include <iostream>

Server *g_server = nullptr;

void request_shutdown(int sig) {
    if (sig == SIGINT && g_server) {
        g_server->requestShutdown();
    }
}

int main(int argc, char *argv[]) {
    try {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " <ip> <port> [--join <ip> <port>]\n";
            return 1;
        }

        std::string server_ip = argv[1];
        uint16_t server_port = std::atoi(argv[2]);
        std::string bootstrap_ip;
        uint16_t bootstrap_port = 0;

        // 检查 --join 参数
        if (argc >= 6 && std::string(argv[3]) == "--join") {
            bootstrap_ip = argv[4];
            bootstrap_port = std::atoi(argv[5]);
        }

        Server server(server_ip, server_port);
        g_server = &server;

        // 注册SIGINT信号处理
        signal(SIGINT, request_shutdown);

        if (!bootstrap_ip.empty()) {
            server.joinCluster(bootstrap_ip, bootstrap_port);
        }

        server.run();
    } catch (const std::exception &e) {
        std::cerr << "CAUGHT EXCEPTION: " << e.what() << '\n';
        std::cerr.flush();
        return 1;
    }
    return 0;
}
