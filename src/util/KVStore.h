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
    void set(std::string key, T value);
    std::optional<T> get(std::string_view key);
    bool del(std::string_view key);
    bool checkexist(std::string_view key);
    KVStore(size_t shardCount = 16);

private:
    size_t shardCount;
    struct Shard {
        HashTable<T> data;
        std::shared_mutex lock;
        LRU lru;
        Shard() : lru(1000) {};
    };
    std::vector<std::unique_ptr<Shard>> shards;
    Shard &getShard(std::string_view key);
};