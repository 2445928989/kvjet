#include "KVStore.h"

KVStore::KVStore(size_t shardCount) : shardCount(shardCount) {
    for (int i = 0; i < shardCount; i++) {
        shards.emplace_back(std::make_unique<Shard>());
    }
}

KVStore::Shard &KVStore::getShard(std::string_view key) {
    auto idx = HashTable::gethash(key) % shardCount;
    return *shards[idx];
}

void KVStore::set(std::string key, std::string value) {
    Shard &shard = getShard(key);
    std::unique_lock lock(shard.lock);
    shard.data.set(key, value);
}
std::optional<std::string> KVStore::get(std::string_view key) {
    Shard &shard = getShard(key);
    std::shared_lock lock(shard.lock);
    return shard.data.get(key);
}
bool KVStore::del(std::string_view key) {
    Shard &shard = getShard(key);
    std::unique_lock lock(shard.lock);
    return shard.data.erase(key);
}
bool KVStore::checkexist(std::string_view key) {
    Shard &shard = getShard(key);
    std::shared_lock lock(shard.lock);
    return shard.data.checkexist(key);
}