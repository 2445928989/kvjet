#pragma once
#include "HashTable.h"
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
class KVStore {
public:
    void set(std::string key, std::string value);
    std::optional<std::string> get(std::string_view key);
    bool del(std::string_view key);
    bool checkexist(std::string_view key);
    KVStore(size_t shardCount = 16);

private:
    size_t shardCount;
    struct Shard {
        HashTable data;
        std::shared_mutex lock;
    };
    std::vector<std::unique_ptr<Shard>> shards;
    Shard &getShard(std::string_view key);
};