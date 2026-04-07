#include "KVStore.h"
#include "../resp/RespValue.h"
#include "HashTable.h"
template <typename T>
KVStore<T>::KVStore(size_t shardCount) : shardCount(shardCount) {
    for (int i = 0; i < shardCount; i++) {
        shards.emplace_back(std::make_unique<Shard>());
    }
}

template <typename T>
typename KVStore<T>::Shard &KVStore<T>::getShard(std::string_view key) {
    auto idx = HashTable<T>::gethash(key) % shardCount;
    return *shards[idx];
}
template <typename T>
std::optional<std::string> KVStore<T>::set(std::string key, T value) {
    Shard &shard = getShard(key);
    std::unique_lock lock(shard.lock);
    shard.data.set(key, std::move(value));
    return std::move(shard.lru.set(std::move(key)));
}

template <typename T>
T* KVStore<T>::get(std::string_view key) {
    Shard &shard = getShard(key);
    std::unique_lock lock(shard.lock);
    auto result = shard.data.get(key);
    if (result != nullptr) {
        shard.lru.access(key);
    }
    return result;
}

template <typename T>
bool KVStore<T>::del(std::string_view key) {
    Shard &shard = getShard(key);
    std::unique_lock lock(shard.lock);
    auto result = shard.data.erase(key);
    if (result) {
        shard.lru.del(key);
    }
    return result;
}

template <typename T>
bool KVStore<T>::checkexist(std::string_view key) {
    Shard &shard = getShard(key);
    std::unique_lock lock(shard.lock);
    return shard.data.checkexist(key);
}

template <typename T>
void KVStore<T>::writetofile(std::string dir) requires (std::same_as<T,resp::RespValue>){
    if(std::filesystem::exists(dir)){
        std::filesystem::remove_all(dir);
    }
    std::filesystem::create_directories(dir);
    for(int i=0;i<shardCount;i++){
        std::unique_lock lock(shards[i]->lock);
        std::filesystem::path path=std::filesystem::path(dir)/("shard_"+std::to_string(i)+".snap");
        shards[i]->data.writetofile(path.string());
    }
}

template <typename T>
void KVStore<T>::readfromfile(std::string dir) requires (std::same_as<T,resp::RespValue>){
    for(int i=0;i<shardCount;i++){
        std::filesystem::path path=std::filesystem::path(dir)/("shard_"+std::to_string(i)+".snap");
        if(!std::filesystem::exists(path)) continue;
        std::unique_lock lock(shards[i]->lock);
        shards[i]->data.readfromfile(path.string());
        shards[i]->lru=LRU(1000);
    }
}

template class KVStore<std::string>;
template class KVStore<resp::RespValue>;