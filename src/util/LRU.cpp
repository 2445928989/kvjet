#include "LRU.h"

LRU::LRU(int maxsz):maxsz(maxsz){}

void LRU::access(std::string_view x){
    auto it=hash.get(x);
    if(it==std::nullopt){
        return;
    }else{
        auto st=*it.value();
        lst.erase(it.value());
        lst.push_back(st);
        hash.set(std::string(x),prev(lst.end()));
    }
}

std::optional<std::string> LRU::set(std::string x){
    auto it=hash.get(x);
    if(it!=std::nullopt){
        auto st=*it.value();
        lst.erase(it.value());
        lst.push_back(st);
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
    if(it==std::nullopt) return;
    lst.erase(it.value());
    hash.erase(x);
}