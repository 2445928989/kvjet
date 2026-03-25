#include "../src/client/Client.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port> <input_file> <expected_output_file>\n";
        return 1;
    }

    std::string ip = argv[1];
    uint16_t port = atoi(argv[2]);
    std::string input_file = argv[3];
    std::string expected_output_file = argv[4];

    try {
        std::ifstream ifs(input_file);
        if (!ifs.is_open()) {
            std::cerr << "Failed to open input file: " << input_file << "\n";
            return 1;
        }

        std::vector<std::string> inputs;
        std::string line;
        while (std::getline(ifs, line)) {
            inputs.push_back(line);
        }
        ifs.close();

        std::ifstream efs(expected_output_file);
        if (!efs.is_open()) {
            std::cerr << "Failed to open expected output file: " << expected_output_file << "\n";
            return 1;
        }

        std::vector<std::string> expected_outputs;
        while (std::getline(efs, line)) {
            expected_outputs.push_back(line);
        }
        efs.close();

        if (inputs.size() != expected_outputs.size()) {
            std::cerr << "Error: input count (" << inputs.size() << ") != expected output count (" << expected_outputs.size() << ")\n";
            return 1;
        }

        std::cout << "Connecting to " << ip << ":" << port << "...\n";
        Client client(ip, port);
        std::cout << "Connected!\n\n";

        // 逐条测试
        for (size_t i = 0; i < inputs.size(); ++i) {
            std::cout << "Test " << (i + 1) << ":\n";
            std::cout << "Sending: \"" << inputs[i] << "\"\n";

            client.send(inputs[i]);
            std::string response = client.recv();

            std::cout << "Received: \"" << response << "\"\n";
            std::cout << "Expected: \"" << expected_outputs[i] << "\"\n";

            if (response == expected_outputs[i]) {
                std::cout << "PASS\n";
            } else {
                std::cout << "FAIL\n";
                return 1;
            }
            std::cout << "\n";
        }

        std::cout << "All tests passed!\n";
        return 0;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
