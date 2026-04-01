#pragma once
#include<string>
#include<mutex>
#include<string>
#include<fstream>
#include<sstream>
#include"KVStore.h"
class AOF{
public:
    //加入text到日志中
    void append(std::string_view text);
    void recover(KVStore<std::string> *kv);
    AOF(std::string filename);
private:
    std::ofstream file;
    std::mutex mut;
    std::string filename;
};