#pragma once
#include "../resp/RespEncoder.h"
#include "../resp/RespParser.h"
#include "../resp/RespValue.h"
#include <concepts>
#include <filesystem>
#include <fstream>
#include <list>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>
template <typename T>
class HashTable {
public:
    HashTable();
    // 获取对应的value,找不到返回std::nullptr
    T *get(std::string_view key);
    // 设置key和value
    void set(std::string key, T value);
    bool erase(std::string_view key);
    static uint32_t gethash(std::string_view key);
    bool checkexist(std::string_view key);
    void writetofile(std::string filename)
        requires(std::same_as<T, resp::RespValue>);
    void readfromfile(std::string filename)
        requires(std::same_as<T, resp::RespValue>);
    // 遍历所有key-value对，callable接收(const std::string&, T*)
    template <typename Func>
    void forEach(Func callback) {
        for (const auto &node : buckets) {
            if (node.status == OCCUPIED) {
                callback(node.key, const_cast<T *>(&node.value));
            }
        }
    }
    enum stat {
        DELETED,
        EMPTY,
        OCCUPIED
    };

private:
    struct Node {
        std::string key;
        T value;
        stat status;
        Node() : key(""), status(EMPTY) {}
        Node(std::string key, T value) : key(std::move(key)), value(std::move(value)), status(OCCUPIED) {}
    };

    std::vector<Node> buckets;
    size_t sz, bucketsz;

    // 扩容
    void rehash();
    // 查找key对应的位置在哪，没有返回第一个DELETED的位置
    Node *find(std::string_view key);
    // 负载因子=元素数量/桶数量
    double loadfactor;
};