// Handler.h
#pragma once
// 用于解析命令
#include "../kvstore/AOF.h"
#include "../resp/RespValue.h"
#include "Server.h"
namespace Handler {
    // 解析命令并返回要发给客户端的字符串
    std::string handle(resp::RespValue request, Server &server, int fd);
    // 仅运行命令，不记录
    std::string handle_noAOF(resp::RespValue request, Server &server);
    // 不记录LRU淘汰情况的SET
    std::string SET_noAOF(resp::RespValue request, Server &server);
    // 需要记录LRU淘汰情况的SET
    std::string SET(resp::RespValue request, Server &server);
    std::string GET(resp::RespValue request, Server &server);
    std::string DEL(resp::RespValue request, Server &server);
    std::string EXIST(resp::RespValue request, Server &server);
    std::string GETNETWORK(Server &server);
    std::string NODEIN(resp::RespValue request, Server &server);
    std::string NODEOUT(resp::RespValue request, Server &server);
    std::string HELLO(resp::RespValue request, Server &server, int fd);
    std::string SYNCDONE(resp::RespValue request, Server &server);
    std::string HEARTBEAT(resp::RespValue request, Server &server, int fd);
    std::string handleRaft(resp::RespValue request, Server &server, int fd);
}