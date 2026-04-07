#include "HashTable.h"
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
T* HashTable<T>::get(std::string_view key) {
    Node *p = find(key);
    if (p->status == OCCUPIED)
        return &p->value;
    else
        return nullptr;
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

template <typename T>
void HashTable<T>::writetofile(std::string filename) requires std::same_as<T, resp::RespValue>{
    std::ofstream file(filename,std::ios::binary);
    if (!file.is_open()) {
        auto p = std::filesystem::path(filename);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        file.open(filename, std::ios::binary);
    }
    if (!file.is_open()) {
        throw std::runtime_error("File open failed.");
    }
    //buckets大小
    uint32_t writebucketsz=bucketsz;
    file.write(reinterpret_cast<const char*>(&writebucketsz),sizeof(writebucketsz));
    //buckets有效元素大小
    uint32_t validsz=sz;
    file.write(reinterpret_cast<const char*>(&validsz),sizeof(validsz));
    for(uint32_t i=0;i<bucketsz;i++){
        const auto &node=buckets[i];
        if(node.status==EMPTY||node.status==DELETED) continue;
        //下标
        file.write(reinterpret_cast<const char*>(&i),sizeof(i));
        //key的大小
        uint32_t keysize=node.key.size();
        file.write(reinterpret_cast<const char*>(&keysize),sizeof(keysize));
        //value的大小
        std::string value=resp::encode(buckets[i].value);
        uint32_t valuesize=value.size();
        file.write(reinterpret_cast<const char*>(&valuesize),sizeof(valuesize));
        //写入key内容
        if(keysize>0){
            file.write(node.key.c_str(),keysize);
        }
        //写入value内容
        if(valuesize>0){
            file.write(value.c_str(),valuesize);
        }
    }
}

template <typename T>
void HashTable<T>::readfromfile(std::string filename) requires std::same_as<T, resp::RespValue>{
    std::ifstream file(filename,std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("File open failed.");
    }
    //buckets大小
    uint32_t readbucketsz;
    file.read(reinterpret_cast<char*>(&readbucketsz),sizeof(readbucketsz));
    bucketsz=readbucketsz;
    buckets.assign(bucketsz,Node());
    //buckets有效元素大小
    uint32_t validsz;
    file.read(reinterpret_cast<char*>(&validsz),sizeof(validsz));
    sz=validsz;
    for(uint32_t i=0;i<validsz;i++){
        //下标
        uint32_t idx;
        file.read(reinterpret_cast<char*>(&idx),sizeof(idx));
        //status
        buckets[idx].status=OCCUPIED;
        //key的大小
        uint32_t keysize;
        file.read(reinterpret_cast<char*>(&keysize),sizeof(keysize));
        //value的大小
        uint32_t valuesize;
        file.read(reinterpret_cast<char*>(&valuesize),sizeof(valuesize));
        //读入key内容
        if(keysize>0){
            buckets[idx].key.resize(keysize);
            file.read(&buckets[idx].key[0],keysize);
        }
        //读入value内容
        if(valuesize>0){
            std::string value;
            value.resize(valuesize);
            file.read(&value[0],valuesize);
            resp::RespParser parser;
            parser.append(value);
            if(!parser.hasResult()){
                throw std::runtime_error("Parser has no result!!!!!");
            }
            buckets[idx].value=parser.getResult().value();
        }
    }
}
template class HashTable<std::string>;
template class HashTable<resp::RespValue>;
template class HashTable<std::list<std::string>::iterator>;