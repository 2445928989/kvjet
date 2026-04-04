#include "KVStore.h"
#include "HashTable1.h"
template<typename T>
KVStore<T>::KVStore(size_t shardCount):shardCount(shardCount){
    for(int i=0;i<shardCount;i++){
        shards.emplace_back(std::make_unique<Shard>());
    }
}

template<typename T>
typename KVStore<T>::Shard& KVStore<T>::getShard(std::string_view key){
    auto idx=HashTable1<T>::gethash(key)%shardCount;
    return *shards[idx];
}
template<typename T>
void KVStore<T>::set(std::string key,T value){
    Shard& shard=getShard(key);
    std::unique_lock lock(shard.lock);
    shard.data.set(key, value);
    shard.lru.set(key);
}

template<typename T>
std::optional<T> KVStore<T>::get(std::string_view key){
    Shard& shard=getShard(key);
    std::unique_lock lock(shard.lock);
    auto result=shard.data.get(key);
    if(result!=std::nullopt){
        shard.lru.access(key);
    }
    return result;
}

template<typename T>
bool KVStore<T>::del(std::string_view key){
    Shard& shard=getShard(key);
    std::unique_lock lock(shard.lock);
    auto result=shard.data.erase(key);
    if(result){
        shard.lru.del(key);
    }
    return result;
}

template<typename T>
bool KVStore<T>::checkexist(std::string_view key){
    Shard& shard=getShard(key);
    std::unique_lock lock(shard.lock);
    return shard.data.checkexist(key);
}

template class KVStore<std::string>;