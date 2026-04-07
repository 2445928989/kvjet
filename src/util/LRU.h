#include<list>
#include<iterator>
#include<optional>
#include"HashTable.h"
#include<string>
class LRU{
public:
    //访问一下key为x的这个元素
    void access(std::string_view x);
    //设置一下key为x的这个元素
    std::optional<std::string> set(std::string x);
    //删除x这个元素
    void del(std::string_view x);
    LRU(int maxsz);
    //清空
    void clear();
private:
    const int maxsz;
    std::list<std::string> lst;
    HashTable<std::list<std::string>::iterator> hash;
};