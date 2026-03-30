#pragma once
#include "KVStore.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
class AOF {
public:
    // 加入text到日志中
    void append(std::string_view text);
    void recover(KVStore *kv);
    AOF(std::string filename);

private:
    std::ofstream file;
    std::mutex mut;
    std::string filename;
};