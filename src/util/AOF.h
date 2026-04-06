#pragma once
#include "../resp/RespValue.h"
#include "KVStore.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
class AOF {
public:
    // 距离上次同步时间超过FSYNC_TIME或缓冲大于BUFFER_SIZE时flush
    static constexpr size_t BUFFER_SIZE = 1024 * 1024;
    static constexpr int FSYNC_TIME = 1000;

    // 加入text到日志中
    void append(std::string_view text);

    void append(resp::RespValue &value);
    // 从日志中恢复
    void recover(KVStore<resp::RespValue> *kv);

    void flush();
    AOF(std::string filename);
    ~AOF();

private:
    std::ofstream file;
    std::mutex mut;
    std::string filename;
    std::chrono::steady_clock::time_point last_fsync;
    std::string buffer;
};