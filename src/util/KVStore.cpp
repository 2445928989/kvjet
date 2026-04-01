#include "KVStore.h"

template<typename T>
KVStore<T>::KVStore(size_t shardCount):shardCount(shardCount){
    for(int i=0;i<shardCount;i++){
        shards.emplace_back(std::make_unique<Shard>());
    }
}

template<typename T>
typename KVStore<T>::Shard& KVStore<T>::getShard(std::string_view key){
    auto idx=HashTable<T>::gethash(key)%shardCount;
    return *shards[idx];
}
template<typename T>
void KVStore<T>::set(std::string key,T value){
    Shard& shard=getShard(key);
    std::unique_lock lock(shard.lock);
    shard.data.set(key,value);
}

template<typename T>
std::optional<T> KVStore<T>::get(std::string_view key){
    Shard& shard=getShard(key);
    std::shared_lock lock(shard.lock);
    return shard.data.get(key);
}

template<typename T>
bool KVStore<T>::del(std::string_view key){
    Shard& shard=getShard(key);
    std::unique_lock lock(shard.lock);
    return shard.data.erase(key);
}

template<typename T>
bool KVStore<T>::checkexist(std::string_view key){
    Shard& shard=getShard(key);
    std::shared_lock lock(shard.lock);
    return shard.data.checkexist(key);
}

template class KVStore<std::string>;