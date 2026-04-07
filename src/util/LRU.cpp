#include "LRU.h"

LRU::LRU(int maxsz):maxsz(maxsz){}

void LRU::access(std::string_view x){
    auto it=hash.get(x);
    if(it==nullptr){
        return;
    }else{
        lst.erase(*it);
        lst.push_back(std::string(x));
        hash.set(std::string(x),prev(lst.end()));
    }
}

std::optional<std::string> LRU::set(std::string x){
    auto it=hash.get(x);
    if(it!=nullptr){
        lst.erase(*it);
        lst.push_back(x);
        hash.set(x,prev(lst.end()));
        return std::nullopt;
    }else{
        lst.emplace_back(x);
        hash.set(x,prev(lst.end()));
        if(lst.size()>maxsz){
            auto erasestr=std::move(*lst.begin());
            lst.erase(lst.begin());
            hash.erase(erasestr);
            return std::move(erasestr);
        }else{
            return std::nullopt;
        }
    }
}

void LRU::del(std::string_view x){
    auto it=hash.get(x);
    if(it==nullptr) return;
    lst.erase(*it);
    hash.erase(x);
}

void LRU::clear(){
    lst.clear();
    hash=HashTable<std::list<std::string>::iterator>();
}