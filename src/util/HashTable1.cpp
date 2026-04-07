#include "HashTable1.h"
#include <mutex>

template<typename T>
HashTable1<T>::HashTable1() : buckets(16), bucketsz(16), sz(0), loadfactor(0.75) {
}

template<typename T>
uint32_t HashTable1<T>::gethash(std::string_view key){
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
typename HashTable1<T>::Node *HashTable1<T>::find(std::string_view key) {
    size_t idx = gethash(key) % bucketsz;
    size_t raw=idx;
    do{
        if(buckets[idx].key==key) return &buckets[idx];
        idx++;
        idx%=bucketsz;
    }while(idx!=raw);
    return nullptr;
}

template<typename T>
size_t HashTable1<T>::findnextfree(std::string_view key) {
    size_t idx = gethash(key) % bucketsz;
    size_t raw=idx;
    do{
        if(buckets[idx].key=="") return idx;
        idx++;
        idx%=bucketsz;
    }while(idx!=raw);
    throw std::runtime_error("cannot find next free pos!");
}

template<typename T>
std::optional<T> HashTable1<T>::get(std::string_view key) {
    Node *p = find(key);
    if (p != nullptr)
        return p->value;
    else
        return std::nullopt;
}

template<typename T>
void HashTable1<T>::set(std::string key,T value) {
    Node *p = find(key);
    if (p != nullptr) {
        p->value = std::move(value);
    } else {
        size_t idx = findnextfree(key);
        buckets[idx]=Node(std::move(key), std::move(value));
        sz++;
        double nowloadfactor = 1.0 * sz / bucketsz;
        if (nowloadfactor > loadfactor)
            rehash();
    }
}

template<typename T>
void HashTable1<T>::rehash() {
    size_t oldbucketsz = bucketsz;
    bucketsz <<= 1;
    decltype(buckets) oldbuckets(bucketsz);
    swap(oldbuckets,buckets);
    for (int i = 0; i < oldbucketsz; i++) {
        auto &node=oldbuckets[i];
        if(node.key=="") continue;
        size_t idx = findnextfree(node.key);
        buckets[idx]=Node(std::move(node.key), std::move(node.value));
    }
}

template<typename T>
bool HashTable1<T>::erase(std::string_view key) {
    auto t = find(key);
    if(t!=nullptr){
        --sz;
        t->key="";
        return true;
    }
    return false;
}

template<typename T>
bool HashTable1<T>::checkexist(std::string_view key){
    return find(key)!=nullptr;
}

template class HashTable1<std::string>;
template class HashTable1<std::list<std::string>::iterator>;