#include "AOF.h"
#include <iostream>
AOF::AOF(std::string filename) : filename(std::move(filename)), file(this->filename, std::ios::app | std::ios::binary) {
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
    file << text << "\n";
    file.flush();
}
void AOF::recover(KVStore *kv) {
    if (kv == nullptr)
        return;
    std::lock_guard lock(mut);
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open())
        return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        std::stringstream sst(line);
        std::string op;
        sst >> op;
        if (op == "SET") {
            std::string key, value;
            sst >> key;
            std::getline(sst, value);
            if (!value.empty() && value.front() == ' ')
                value = value.substr(1);
            kv->set(std::move(key), std::move(value));
        } else if (op == "DEL") {
            std::string key;
            sst >> key;
            kv->del(key);
        }
    }
}
