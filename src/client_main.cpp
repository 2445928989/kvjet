// client_main.cpp
#include "client/Client.h"
#include <iostream>
#include <string>
int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <ip> <port>\n";
        return 1;
    }
    try {
        Client client(std::string(argv[1]), atoi(argv[2]));
        client.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}