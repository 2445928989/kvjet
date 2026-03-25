// server_main.cpp
#include "server/Server.h"
#include <iostream>
int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }
    try {
        Server server(atoi(argv[1]));
        server.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}