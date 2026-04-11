#pragma once
#include "../config/Config.h"
#include "HashTable.h"
#include "LRU.h"
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
template <typename T>
class KVStore {
public:
    // 若LRI有淘汰键，则返回
    std::optional<std::string> set(std::string key, T value);
    T *get(std::string_view key);
    bool del(std::string_view key);
    bool checkexist(std::string_view key);
    KVStore(size_t shardCount = Config::SHARD_COUNT);
    // 写入快照到文件，传入路径
    void writetofile(std::string dir)
        requires(std::same_as<T, resp::RespValue>);
    // 从文件读入快照，传入路径
    void readfromfile(std::string dir)
        requires(std::same_as<T, resp::RespValue>);
    // 遍历所有key-value对，callable接收(const std::string&, T*)
    template <typename Func>
    void forEach(Func callback) {
        for (auto &shard : shards) {
            std::shared_lock lock(shard->lock);
            shard->data.forEach(callback);
        }
    }

private:
    size_t shardCount;
    struct Shard {
        HashTable<T> data;
        std::shared_mutex lock;
        LRU lru;
        Shard() : lru(Config::LRU_MAX_SIZE) {};
    };
    std::vector<std::unique_ptr<Shard>> shards;
    Shard &getShard(std::string_view key);
};