// server_main.cpp
    #include "config/Config.h"
#include "server/Server.h"
#include <iostream>
int main(int argc, char *argv[]) {
    try {
        Server server(Config::SERVER_PORT);
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "CAUGHT EXCEPTION: " << e.what() << '\n';
        std::cerr.flush();
        return 1;
    } catch (...) {
        std::cerr << "CAUGHT UNKNOWN EXCEPTION\n";
        std::cerr.flush();
        return 1;
    }
    return 0;
}