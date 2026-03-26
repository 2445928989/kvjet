#include "HashTable.h"
#include <mutex>
HashTable::HashTable() : buckets(16), bucketsz(16), sz(0), loadfactor(0.75) {
}

uint32_t HashTable::gethash(std::string_view key){
    uint32_t h = 0;
    for (const char &c : key) {
        h = h * 131 + static_cast<unsigned char>(c);
    }
    h ^= h >> 16;
    h *= 0x7feb352d;
    h ^= h >> 15;
    return h;
}
HashTable::Node *HashTable::find(std::string_view key) {
    size_t idx = gethash(key) % bucketsz;
    for (auto &node : buckets[idx]) {
        if (node.key == key)
            return &node;
    }
    return nullptr;
}

std::optional<std::string> HashTable::get(std::string_view key) {
    Node *p = find(key);
    if (p != nullptr)
        return p->value;
    else
        return std::nullopt;
}

void HashTable::set(std::string key, std::string value) {
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

void HashTable::rehash() {
    size_t oldbucketsz = bucketsz;
    bucketsz <<= 1;
    decltype(buckets) newbuckets(bucketsz);
    for (int i = 0; i < oldbucketsz; i++) {
        for (auto &node : buckets[i]) {
            size_t idx = gethash(node.key) % bucketsz;
            newbuckets[idx].push_back({std::move(node.key), std::move(node.value)});
        }
    }
    buckets.swap(newbuckets);
}

bool HashTable::erase(std::string_view key) {
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

bool HashTable::checkexist(std::string_view key){
    return find(key)!=nullptr;
}