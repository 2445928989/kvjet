// Handler.cpp
#include "Handler.h"
#include "../kvstore/AOF.h"
#include "../resp/RespEncoder.h"
#include "../util/Utils.h"
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
std::string Handler::handle(resp::RespValue request, Server &server, int fd) {
    if (auto it = std::get_if<resp::Array>(request.getPtr())) {
        if (it->value->size() == 0) {
            throw std::runtime_error("Empty request!!");
        }
        if (auto command = std::get_if<resp::SimpleString>((*it->value->begin())->getPtr())) {
            if (command->value == "SYNCDONE") {
                return SYNCDONE(std::move(request), server);
            }
            if (command->value == "HEARTBEAT") {
                return HEARTBEAT(std::move(request), server, fd);
            }
            // 同步期间拒绝客户端数据命令，集群内部命令照常处理
            if (server.isSyncing() && (command->value == "SET" || command->value == "GET" ||
                                       command->value == "DEL" || command->value == "EXIST")) {
                return resp::encode(resp::RespValue(resp::Error("LOADING server is syncing, please retry")));
            }
            if (command->value == "GET") {
                return GET(std::move(request), server);
            } else if (command->value == "SET") {
                server.getAOF().append(request);
                return SET(std::move(request), server);
            } else if (command->value == "DEL") {
                server.getAOF().append(request);
                return DEL(std::move(request), server);
            } else if (command->value == "EXIST") {
                return EXIST(std::move(request), server);
            } else if (command->value == "NODEIN") {
                return NODEIN(std::move(request), server);
            } else if (command->value == "NODEOUT") {
                return NODEOUT(std::move(request), server);
            } else if (command->value == "GETNETWORK") {
                return GETNETWORK(server);
            } else if (command->value == "HELLO") {
                return HELLO(std::move(request), server, fd);
            } else {
                throw std::runtime_error("Unknown request!!");
            }
        } else {
            throw std::runtime_error("Command is not a SimpleString!!");
        }
    } else if (auto it = std::get_if<resp::SimpleString>(request.getPtr())) {
        if (it->value == "OK") {
            return "";
        } else {
            throw std::runtime_error("Unknown request!");
        }
    } else {
        throw std::runtime_error("Unknown request!");
    }
}

std::string Handler::handle_noAOF(resp::RespValue request, Server &server) {
    if (auto it = std::get_if<resp::Array>(request.getPtr())) {
        if (it->value->size() == 0) {
            throw std::runtime_error("Empty request!!");
        }
        if (auto command = std::get_if<resp::SimpleString>((*it->value->begin())->getPtr())) {
            if (command->value == "GET") {
                return GET(std::move(request), server);
            } else if (command->value == "SET") {
                return SET_noAOF(std::move(request), server);
            } else if (command->value == "DEL") {
                return DEL(std::move(request), server);
            } else if (command->value == "EXIST") {
                return EXIST(std::move(request), server);
            } else {
                throw std::runtime_error("Unknown request!!");
            }
        } else {
            throw std::runtime_error("Command is not a SimpleString!!");
        }
    } else {
        throw std::runtime_error("Request is not an Array!!");
    }
}

std::string Handler::SET_noAOF(resp::RespValue request, Server &server) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 3) {
        throw std::runtime_error("SET: Wrong number of arguments");
    }
    auto key = std::move(it->value.value()[1]);
    auto value = std::move(it->value.value()[2]);
    if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
        auto deleted_key = server.getKVStore().set(std::move(key_->value), std::move(*value));
        if (deleted_key.has_value()) {
        }
        resp::RespValue ret(resp::SimpleString("OK"));
        return resp::encode(ret);
    } else {
        throw std::runtime_error("SET: Key is not a SimpleString");
    }
}

std::string Handler::SET(resp::RespValue request, Server &server) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 3) {
        throw std::runtime_error("SET: Wrong number of arguments");
    }
    auto key = std::move(it->value.value()[1]);
    auto value = std::move(it->value.value()[2]);
    if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
        auto deleted_key = server.getKVStore().set(std::move(key_->value), std::move(*value));
        if (deleted_key.has_value()) {
            server.getAOF().append(std::string("*2\r\n+DEL\r\n+" + deleted_key.value() + "\r\n"));
        }
        resp::RespValue ret(resp::SimpleString("OK"));
        return resp::encode(ret);
    } else {
        throw std::runtime_error("SET: Key is not a SimpleString");
    }
}

std::string Handler::GET(resp::RespValue request, Server &server) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 2) {
        throw std::runtime_error("GET: Wrong number of arguments");
    }
    auto key = std::move(it->value.value()[1]);
    if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
        auto result = server.getKVStore().get(std::move(key_->value));
        if (result != nullptr) {
            return resp::encode(*result);
        } else {
            resp::RespValue ret(resp::BulkString(std::nullopt));
            return resp::encode(ret);
        }
    } else {
        throw std::runtime_error("GET: Key is not a SimpleString");
    }
}

std::string Handler::DEL(resp::RespValue request, Server &server) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 2) {
        throw std::runtime_error("DEL: Usage: DEL key");
    }
    auto key = std::move(it->value.value()[1]);
    if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
        int64_t ret = server.getKVStore().del(std::move(key_->value));
        return resp::encode(resp::RespValue(ret));
    } else {
        throw std::runtime_error("DEL: Key is not a SimpleString");
    }
}

std::string Handler::EXIST(resp::RespValue request, Server &server) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 2) {
        throw std::runtime_error("EXIST: Usage: EXIST key");
    }
    auto key = std::move(it->value.value()[1]);
    if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
        int64_t ret = server.getKVStore().get(std::move(key_->value)) != nullptr;
        return resp::encode(resp::RespValue(ret));
    } else {
        throw std::runtime_error("EXIST: Key is not a SimpleString");
    }
}

std::string Handler::GETNETWORK(Server &server) {

    return resp::encodeTopo(server.getCluster().getTopo());
}

// 简直是地狱。
std::string Handler::NODEIN(resp::RespValue request, Server &server) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 3) {
        throw std::runtime_error("NODEIN: Wrong number of arguments");
    }
    // 先检查这个gossip的UUID，有没有已经接受过
    auto uuid = std::move(it->value.value()[1]);
    if (auto uuid_ = std::get_if<resp::SimpleString>(uuid->getPtr())) {
        uint64_t id = Utils::to_uint64_t(uuid_->value);
        if (server.getCluster().findGossip(id)) {
            return "";
        }
        server.getCluster().addGossip(id);
    } else {
        throw std::runtime_error("NODEIN: UUID is not a SimpleString");
    }
    it->value.value()[1] = std::move(uuid);
    // 广播消息
    auto conn = server.getCluster().getConnections();
    for (auto &[uuid, fd] : conn) {
        std::unique_lock<std::mutex> lock(server.getQueueMutex());
        server.getMessageQueue().emplace(resp::encode(request), fd);
    }

    // 得到新接入节点的信息
    auto node = std::move(it->value.value()[2]);
    Cluster::Node new_node = Utils::getNode(std::move(*node));

    // 已经接入过就不要重新接入了
    if (server.getCluster().getConnection(new_node.UUID) != -1) {
        server.getCluster().addNodeToHash(new_node.UUID);
        server.getCluster().addTopoNode(std::move(new_node));
        server.sendDataToNode(new_node.UUID);
        return "";
    }
    {
        std::unique_lock<std::mutex> lock(server.getQueueMutex());
        server.getMessageQueue().emplace(new_node.ip + " " + std::to_string(new_node.port) + " " + std::to_string(new_node.UUID), -1);
    }
    // 把这个节点加入拓扑和一致性哈希
    server.getCluster().addNodeToHash(new_node.UUID);
    server.getCluster().addTopoNode(std::move(new_node));

    // 以消息队列的形式告诉主线程去连一下它

    return "";
}
std::string Handler::NODEOUT(resp::RespValue request, Server &server) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 3) {
        throw std::runtime_error("NODEOUT: Wrong number of arguments");
    }
    // 先检查gossip的UUID，有没有接受过
    auto uuid = std::move(it->value.value()[1]);
    if (auto uuid_ = std::get_if<resp::SimpleString>(uuid->getPtr())) {
        uint64_t id = Utils::to_uint64_t(uuid_->value);
        if (server.getCluster().findGossip(id)) {
            return "";
        }
        server.getCluster().addGossip(id);
    } else {
        throw std::runtime_error("NODEOUT:UUID is not a SimpleString");
    }
    it->value.value()[1] = std::move(uuid);
    // 广播它离开的消息（跳过自己）
    auto conn = server.getCluster().getConnections();
    for (auto &[uuid, fd] : conn) {
        std::unique_lock<std::mutex> lock(server.getQueueMutex());
        server.getMessageQueue().emplace(resp::encode(request), fd);
    }
    // 获取离开集群的node的UUID
    auto node_id = std::move(it->value.value()[2]);
    if (auto node_id_ = std::get_if<resp::SimpleString>(node_id->getPtr())) {
        int uuid = Utils::to_uint64_t(node_id_->value);
        // 从一致性哈希和拓扑中删除它
        server.getCluster().delNodeToHash(uuid);
        server.getCluster().delTopoNode(uuid);
        server.getCluster().delConnection(uuid);
        return "";
    } else {
        throw std::runtime_error("NODEOUT: node_id is not a SimpleString");
    }
}
std::string Handler::HEARTBEAT(resp::RespValue request, Server &server, int fd) {
    server.getCluster().updateHeartbeat(fd);
    return "";
}

std::string Handler::SYNCDONE(resp::RespValue request, Server &server) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() < 2) {
        throw std::runtime_error("SYNCDONE: missing node UUID");
    }
    server.onSyncDone();
    return "";
}
std::string Handler::HELLO(resp::RespValue request, Server &server, int fd) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 2) {
        throw std::runtime_error("HELLO: Wrong number of arguments");
    }
    auto UUID = std::move(it->value.value()[1]);
    if (auto UUID_ = std::get_if<resp::SimpleString>(UUID->getPtr())) {
        server.getCluster().addConnection(Utils::to_uint64_t(UUID_->value), fd);
        resp::RespValue ret(resp::SimpleString("OK"));
        return resp::encode(ret);
    } else {
        throw std::runtime_error("HELLO: UUID is not a SimpleString");
    }
}