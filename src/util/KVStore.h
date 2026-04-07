#pragma once
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
    T* get(std::string_view key);
    bool del(std::string_view key);
    bool checkexist(std::string_view key);
    KVStore(size_t shardCount = 16);
    //写入快照到文件，传入路径
    void writetofile(std::string dir) requires (std::same_as<T,resp::RespValue>);
    //从文件读入快照，传入路径
    void readfromfile(std::string dir) requires (std::same_as<T,resp::RespValue>);
private:
    size_t shardCount;
    const int lrusz;
    struct Shard {
        HashTable<T> data;
        std::shared_mutex lock;
        LRU lru;
        Shard() : lru(lrusz) {};
    };
    std::vector<std::unique_ptr<Shard>> shards;
    Shard &getShard(std::string_view key);
};