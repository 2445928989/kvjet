#include<list>
#include<iterator>
#include<optional>
#include"HashTable.h"
#include<string>
class LRU{
public:
    //访问一下key为x的这个元素
    std::optional<std::string> access(std::string x);
    //删除x这个元素
    void del(std::string x);
    LRU(int maxsz);
private:
    const int maxsz;
    std::list<std::string> lst;
    HashTable<std::list<std::string>::iterator> hash;
};