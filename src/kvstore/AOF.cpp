#include "AOF.h"
#include "../resp/RespEncoder.h"
#include "../resp/RespParser.h"
#include "../server/Handler.h"
#include <iostream>
#include <unistd.h>
AOF::AOF(std::string filename) : filename(std::move(filename)), file(this->filename, std::ios::app | std::ios::binary), last_fsync(std::chrono::steady_clock::now()) {
    if (!file.is_open()) {
        auto p = std::filesystem::path(this->filename);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        file.open(this->filename, std::ios::app | std::ios::binary);
    }
    if (!file.is_open()) {
        throw std::runtime_error("AOF open failed.");
    }
}
void AOF::append(std::string_view text) {
    std::lock_guard lock(mut);
    auto now = std::chrono::steady_clock::now();
    buffer += text;
    if (buffer.size() > Config::AOF_BUFFER_SIZE ||
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fsync).count() > Config::AOF_FSYNC_TIME_MS) {
        flush();
    }
}
void AOF::append(resp::RespValue &value) {
    std::lock_guard lock(mut);
    auto now = std::chrono::steady_clock::now();
    buffer += std::move(resp::encode(value));
    if (buffer.size() > Config::AOF_BUFFER_SIZE ||
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fsync).count() > Config::AOF_FSYNC_TIME_MS) {
        flush();
    }
}
void AOF::recover(Server &server) {
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open())
        return;
    int count = 0;
    char buf[Config::AOF_BUFFER_SIZE];
    resp::RespParser parser;
    while (in.read(buf, sizeof(buf)) || in.gcount() > 0) {
        size_t sz = in.gcount();
        parser.append(std::string(buf, sz));
        while (parser.hasResult()) {
            auto cmd = parser.getResult().value();
            Handler::handle_noAOF(std::move(cmd), server);
            count++;
        }
    }
    std::cout << "Recovered " << count << " commands from AOF\n";
}

void AOF::flush() {
    file << buffer;
    file.flush();
    buffer.clear();
    last_fsync = std::chrono::steady_clock::now();
}

AOF::~AOF() {
    std::lock_guard lock(mut);
    flush();
}