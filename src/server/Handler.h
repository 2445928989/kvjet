// Handler.h
#pragma once
// 用于解析命令
#include "../kvstore/AOF.h"
#include "../kvstore/KVStore.h"
#include "../resp/RespValue.h"
namespace Handler {
    // 解析命令并返回要发给客户端的字符串
    std::string handle(resp::RespValue request, KVStore<resp::RespValue> &kvstore, AOF &aof);
    // 仅运行命令，不记录
    std::string handle(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    std::string SET(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    // 需要记录LRU淘汰情况
    std::string SET(resp::RespValue request, KVStore<resp::RespValue> &kvstore, AOF &aof);
    std::string GET(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    std::string DEL(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    std::string EXISTS(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    std::string MGET(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
}