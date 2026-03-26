#include <list>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>
class HashTable {
public:
    HashTable();
    // 获取对应的value,找不到返回std::nullopt
    std::optional<std::string> get(std::string_view key);
    // 设置key和value
    void set(std::string key, std::string value);
    bool erase(std::string_view key);
    static uint32_t gethash(std::string_view key);
    bool checkexist(std::string_view key);
private:
    struct Node {
        std::string key;
        std::string value;
    };

    std::vector<std::list<Node>> buckets;
    size_t sz, bucketsz;

    // 扩容
    void rehash();
    // 查找key对应的位置在哪，没有返回nullptr
    Node *find(std::string_view key);
    // 负载因子=元素数量/桶数量
    double loadfactor;
};