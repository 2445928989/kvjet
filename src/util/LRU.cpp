#include "LRU.h"

LRU::LRU(int maxsz):maxsz(maxsz){}

std::optional<std::string> LRU::access(std::string x){
    auto result=hash.get(x);
    if(result!=std::nullopt){
        
    }else{

    }
}

void LRU::del(std::string x){

}