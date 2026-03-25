#include<vector>
#include<list>
#include<string>
#include<optional>
class HashTable{
public:
    HashTable();
    //获取对应的value,找不到返回std::nullopt
    std::optional<std::string> get(std::string key);
    //设置key和value
    void set(std::string key,std::string value);
    bool erase(std::string key);
private:
    struct Node{
        std::string key;
        std::string value;
    };
    std::vector<std::list<Node>> buckets;
    int sz,bucketsz;
    int gethash(std::string key);
    void rehash();
    //查找key对应的位置在哪，没有返回nullptr
    Node* find(std::string key);
    //负载因子=元素数量/桶数量
    double loadfactor;
};