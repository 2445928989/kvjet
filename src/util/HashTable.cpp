#include "HashTable.h"
#include <mutex>
#include "../resp/RespValue.h"
template<typename T>
HashTable<T>::HashTable() : buckets(16), bucketsz(16), sz(0), loadfactor(0.75) {
}

template<typename T>
uint32_t HashTable<T>::gethash(std::string_view key){
    uint32_t h = 0;
    for (const char &c : key) {
        h = h * 131 + static_cast<unsigned char>(c);
    }
    h ^= h >> 16;
    h *= 0x7feb352d;
    h ^= h >> 15;
    return h;
}

template<typename T>
typename HashTable<T>::Node *HashTable<T>::find(std::string_view key) {
    size_t idx = gethash(key) % bucketsz;
    for (auto &node : buckets[idx]) {
        if (node.key == key)
            return &node;
    }
    return nullptr;
}

template<typename T>
std::optional<T> HashTable<T>::get(std::string_view key) {
    Node *p = find(key);
    if (p != nullptr)
        return std::optional<T>(std::move(p->value));
    else
        return std::nullopt;
}

template<typename T>
void HashTable<T>::set(std::string key,T value) {
    Node *p = find(key);
    if (p != nullptr) {
        p->value = std::move(value);
    } else {
        size_t idx = gethash(key) % bucketsz;
        buckets[idx].push_back({std::move(key), std::move(value)});
        sz++;
        double nowloadfactor = 1.0 * sz / bucketsz;
        if (nowloadfactor > loadfactor)
            rehash();
    }
}

template<typename T>
void HashTable<T>::rehash() {
    size_t oldbucketsz = bucketsz;
    bucketsz <<= 1;
    decltype(buckets) newbuckets(bucketsz);
    for (size_t i = 0; i < oldbucketsz; i++) {
        for (auto &node : buckets[i]) {
            size_t idx = gethash(node.key) % bucketsz;
            newbuckets[idx].push_back({std::move(node.key), std::move(node.value)});
        }
    }
    buckets.swap(newbuckets);
}

template<typename T>
bool HashTable<T>::erase(std::string_view key) {
    size_t idx = gethash(key) % bucketsz;
    for (auto it = buckets[idx].begin(); it != buckets[idx].end(); it++) {
        if (it->key == key) {
            buckets[idx].erase(it);
            --sz;
            return true;
        }
    }
    return false;
}

template<typename T>
bool HashTable<T>::checkexist(std::string_view key){
    return find(key)!=nullptr;
}

template class HashTable<std::string>;
template class HashTable<resp::RespValue>;
template class HashTable<std::list<std::string>::iterator>;