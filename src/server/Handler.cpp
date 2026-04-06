// Handler.cpp
#include "Handler.h"
#include "../resp/RespEncoder.h"
#include "../util/AOF.h"
#include <stdexcept>
std::string Handler::handle(resp::RespValue request, KVStore<resp::RespValue> &kvstore, AOF &aof) {
    if (auto it = std::get_if<resp::Array>(request.getPtr())) {
        if (it->value->size() == 0) {
            throw std::runtime_error("Empty request!!");
        }
        if (auto command = std::get_if<resp::SimpleString>((*it->value->begin())->getPtr())) {
            if (command->value == "GET") {
                return GET(std::move(request), kvstore);
            } else if (command->value == "SET") {
                aof.append(request);
                return SET(std::move(request), kvstore, aof);
            } else if (command->value == "DEL") {
                aof.append(request);
                return DEL(std::move(request), kvstore);
            } else if (command->value == "EXISTS") {
                return EXISTS(std::move(request), kvstore);
            } else if (command->value == "MGET") {
                return MGET(std::move(request), kvstore);
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
std::string Handler::handle(resp::RespValue request, KVStore<resp::RespValue> &kvstore) {
    if (auto it = std::get_if<resp::Array>(request.getPtr())) {
        if (it->value->size() == 0) {
            throw std::runtime_error("Empty request!!");
        }
        if (auto command = std::get_if<resp::SimpleString>((*it->value->begin())->getPtr())) {
            if (command->value == "GET") {
                return GET(std::move(request), kvstore);
            } else if (command->value == "SET") {
                return SET(std::move(request), kvstore);
            } else if (command->value == "DEL") {
                return DEL(std::move(request), kvstore);
            } else if (command->value == "EXISTS") {
                return EXISTS(std::move(request), kvstore);
            } else if (command->value == "MGET") {
                return MGET(std::move(request), kvstore);
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
std::string Handler::SET(resp::RespValue request, KVStore<resp::RespValue> &kvstore) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 3) {
        throw std::runtime_error("SET: Wrong number of arguments");
    }
    auto key = std::move(it->value.value()[1]);
    auto value = std::move(it->value.value()[2]);
    if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
        auto deleted_key = std::move(kvstore.set(std::move(key_->value), std::move(*value)));
        if (deleted_key.has_value()) {
        }
        resp::RespValue ret(resp::SimpleString("OK"));
        return resp::encode(ret);
    } else {
        throw std::runtime_error("SET: Key is not a SimpleString");
    }
}
std::string Handler::SET(resp::RespValue request, KVStore<resp::RespValue> &kvstore, AOF &aof) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 3) {
        throw std::runtime_error("SET: Wrong number of arguments");
    }
    auto key = std::move(it->value.value()[1]);
    auto value = std::move(it->value.value()[2]);
    if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
        auto deleted_key = std::move(kvstore.set(std::move(key_->value), std::move(*value)));
        if (deleted_key.has_value()) {
            aof.append(std::string("*2\r\n+DEL\r\n+" + deleted_key.value() + "\r\n"));
        }
        resp::RespValue ret(resp::SimpleString("OK"));
        return resp::encode(ret);
    } else {
        throw std::runtime_error("SET: Key is not a SimpleString");
    }
}
std::string Handler::GET(resp::RespValue request, KVStore<resp::RespValue> &kvstore) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    if (it->value->size() != 2) {
        throw std::runtime_error("GET: Wrong number of arguments");
    }
    auto key = std::move(it->value.value()[1]);
    if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
        auto result = kvstore.get(std::move(key_->value));
        if (result.has_value()) {
            return resp::encode(result.value());
        } else {
            resp::RespValue ret(resp::BulkString(std::nullopt));
            return resp::encode(ret);
        }
    } else {
        throw std::runtime_error("GET: Key is not a SimpleString");
    }
}
std::string Handler::DEL(resp::RespValue request, KVStore<resp::RespValue> &kvstore) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    size_t sz = it->value->size();
    int64_t ret = 0;
    if (sz == 1) {
        throw std::runtime_error("DEL: At least one key");
    }
    for (size_t i = 1; i < sz; i++) {
        auto key = std::move(it->value.value()[i]);
        if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
            ret += kvstore.del(std::move(key_->value));
        } else {
            throw std::runtime_error("DEL: Key is not a SimpleString");
        }
    }
    return resp::encode(resp::RespValue(ret));
}
std::string Handler::EXISTS(resp::RespValue request, KVStore<resp::RespValue> &kvstore) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    size_t sz = it->value->size();
    int64_t ret = 0;
    if (sz == 1) {
        throw std::runtime_error("EXISTS: At least one key");
    }
    for (size_t i = 1; i < sz; i++) {
        auto key = std::move(it->value.value()[i]);
        if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
            ret += kvstore.get(std::move(key_->value)).has_value();
        } else {
            throw std::runtime_error("EXISTS: Key is not a SimpleString");
        }
    }
    return resp::encode(resp::RespValue(ret));
}
std::string Handler::MGET(resp::RespValue request, KVStore<resp::RespValue> &kvstore) {
    auto it = std::get_if<resp::Array>(request.getPtr());
    size_t sz = it->value->size();
    resp::Array ret;
    if (sz == 1) {
        throw std::runtime_error("MGET: At least one key");
    }
    for (size_t i = 1; i < sz; i++) {
        auto key = std::move(it->value.value()[i]);
        if (auto key_ = std::get_if<resp::SimpleString>(key->getPtr())) {
            auto result = kvstore.get(std::move(key_->value));
            if (result.has_value()) {
                ret.value->push_back(std::make_unique<resp::RespValue>(std::move(result.value())));
            } else {
                ret.value->push_back(std::make_unique<resp::RespValue>(resp::BulkString(nullptr)));
            }
        } else {
            throw std::runtime_error("MGET: Key is not a SimpleString");
        }
    }
    return resp::encode(resp::RespValue(std::move(ret)));
}