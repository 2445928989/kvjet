// Handler.h
#pragma once
// 用于解析命令
#include "../resp/RespValue.h"
#include "../util/KVStore1.h"
namespace Handler {
    // 解析命令并返回要发给客户端的字符串
    std::string handle(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    std::string SET(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    std::string GET(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    std::string DEL(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    std::string EXISTS(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
    std::string MGET(resp::RespValue request, KVStore<resp::RespValue> &kvstore);
}