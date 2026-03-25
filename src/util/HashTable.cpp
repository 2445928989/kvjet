#include "HashTable.h"

HashTable::HashTable():buckets(16),bucketsz(16),sz(0),loadfactor(0.75){

}

int HashTable::gethash(std::string key){
    unsigned int h=0;
    for(char c:key){
        h=h*131+static_cast<unsigned char>(c);
    }
    return static_cast<int>(h);
}
HashTable::Node* HashTable::find(std::string key){
    int idx=gethash(key)%bucketsz;
    for(auto &node:buckets[idx]){
        if(node.key==key) return &node;
    }
    return nullptr;
}

std::optional<std::string> HashTable::get(std::string key){
    Node *p=find(key);
    if(p!=nullptr) return p->value;
    else return std::nullopt;
}

void HashTable::set(std::string key,std::string value){
    Node *p=find(key);
    if(p!=nullptr){
        p->value=std::move(value);
    }else{
        int idx=gethash(key)%bucketsz;
        buckets[idx].push_back({std::move(key),std::move(value)});
        sz++;
        double nowloadfactor=1.0*sz/bucketsz;
        if(nowloadfactor>loadfactor) rehash();
    }
}

void HashTable::rehash(){
    int oldbucketsz=bucketsz;
    bucketsz<<=1;
    decltype(buckets) newbuckets(bucketsz);
    for(int i=0;i<oldbucketsz;i++){
        for(auto &node:buckets[i]){
            int idx=gethash(node.key)%bucketsz;
            newbuckets[idx].push_back({std::move(node.key),std::move(node.value)});
        }
    }
    buckets.swap(newbuckets);
}

bool HashTable::erase(std::string key){
    int idx=gethash(key)%bucketsz;
    for(auto it=buckets[idx].begin();it!=buckets[idx].end();it++){
        if(it->key==key){
            buckets[idx].erase(it);
            --sz;
            return true;
        }
    }
    return false;
}