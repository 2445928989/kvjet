#include "HashTable.h"
#include "../resp/RespValue.h"
#include <mutex>
#include <stdexcept>
template <typename T>
HashTable<T>::HashTable() : buckets(16), bucketsz(16), sz(0), loadfactor(0.75) {
}

template <typename T>
uint32_t HashTable<T>::gethash(std::string_view key) {
    uint32_t h = 2654435761U;
    for (char c : key) {
        h ^= (uint32_t)c;
        h *= 16777619U;
    }
    return h;
}

template <typename T>
typename HashTable<T>::Node *HashTable<T>::find(std::string_view key) {
    size_t idx = gethash(key) % bucketsz;
    size_t raw = idx;
    Node *first_del = nullptr;
    do {
        if (buckets[idx].status == EMPTY) {
            if (first_del == nullptr)
                return &buckets[idx];
            else
                return first_del;
        }
        if (buckets[idx].status == DELETED) {
            if (first_del == nullptr)
                first_del = &buckets[idx];
        } else if (buckets[idx].key == key)
            return &buckets[idx];
        idx++;
        idx %= bucketsz;
    } while (idx != raw);
    throw std::runtime_error("Empty position not found");
}

template <typename T>
std::optional<T> HashTable<T>::get(std::string_view key) {
    Node *p = find(key);
    if (p->status == OCCUPIED)
        return std::optional<T>(std::move(p->value));
    else
        return std::nullopt;
}

template <typename T>
void HashTable<T>::set(std::string key, T value) {
    Node *p = find(key);
    if (p->status == OCCUPIED) {
        p->value = std::move(value);
    } else {
        *p = Node(std::move(key), std::move(value));
        sz++;
        double nowloadfactor = 1.0 * sz / bucketsz;
        if (nowloadfactor > loadfactor)
            rehash();
    }
}

template <typename T>
void HashTable<T>::rehash() {
    size_t oldbucketsz = bucketsz;
    bucketsz <<= 1;
    decltype(buckets) oldbuckets(bucketsz);
    swap(oldbuckets, buckets);
    for (int i = 0; i < oldbucketsz; i++) {
        auto &node = oldbuckets[i];
        if (node.status != OCCUPIED)
            continue;
        Node *p = find(node.key);
        *p = {std::move(node.key), std::move(node.value)};
    }
}

template <typename T>
bool HashTable<T>::erase(std::string_view key) {
    Node *t = find(key);
    if (t->status == OCCUPIED) {
        --sz;
        t->status = DELETED;
        return true;
    }
    return false;
}

template <typename T>
bool HashTable<T>::checkexist(std::string_view key) {
    return find(key)->status == OCCUPIED;
}

template class HashTable<std::string>;
template class HashTable<resp::RespValue>;
template class HashTable<std::list<std::string>::iterator>;