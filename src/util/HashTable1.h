#pragma once
#include <list>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>
template<typename T>
class HashTable1 {
public:
    HashTable1();
    // 获取对应的value,找不到返回std::nullopt
    std::optional<T> get(std::string_view key);
    // 设置key和value
    void set(std::string key, T value);
    bool erase(std::string_view key);
    static uint32_t gethash(std::string_view key);
    bool checkexist(std::string_view key);
private:
    struct Node {
        std::string key;
        T value;
        Node():key(""){}
        Node(std::string key,T value):key(std::move(key)),value(std::move(value)){}
    };

    std::vector<Node> buckets;
    size_t sz, bucketsz;

    // 扩容
    void rehash();
    // 查找key对应的位置在哪，没有返回nullptr
    Node *find(std::string_view key);
    //找下一个可以放的位置
    size_t findnextfree(std::string_view key);
    // 负载因子=元素数量/桶数量
    double loadfactor;
};