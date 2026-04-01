// RespParser.cpp
#include "RespParser.h"
#include <cstring>
#include <stdexcept>
using namespace resp;
namespace resp {
    RespParser::RespParser() : state(WaitType), node() {
    }
    void RespParser::append(const std::string& app) {
        for (const char &c : app) {
            switch (state) {
            case WaitType:
                if (c == '+') {
                    node = RespState(std::move(RespValue(SimpleString(std::string()))), 0);
                    auto it = std::get_if<SimpleString>(node.value.getPtr());
                    it->value.reserve(32);
                    state = ReadLine;
                } else if (c == '-') {
                    node = RespState(std::move(RespValue(Error(std::string()))), 0);
                    auto it = std::get_if<Error>(node.value.getPtr());
                    it->value.reserve(32);
                    state = ReadLine;
                } else if (c == ':') {
                    node = RespState(std::move(RespValue(int64_t(0))), 0);
                    state = ReadLine;
                } else if (c == '$') {
                    node = RespState(std::move(RespValue(BulkString())), 0);
                    state = ReadSize;
                } else if (c == '*') {
                    node = RespState(std::move(RespValue(Array())), 0);
                    state = ReadSize;
                } else {
                    throw std::runtime_error("Parser Error!!");
                }
                break;
            case ReadSize:
                if (c == '-') {
                    if (auto it = std::get_if<Array>(node.value.getPtr())) {
                        if (it->value.has_value()) {
                            throw std::runtime_error("Parser Error!!");
                        }
                    } else if (auto it = std::get_if<BulkString>(node.value.getPtr())) {
                        if (it->value.has_value()) {
                            throw std::runtime_error("Parser Error!!");
                        }
                    } else {
                        throw std::runtime_error("Parser Error!!");
                    }
                    state = WaitNil;
                } else if (c == '\r') {
                    if (auto it = std::get_if<Array>(node.value.getPtr())) {
                        it->value->reserve(node.total);
                    } else if (auto it = std::get_if<BulkString>(node.value.getPtr())) {
                        it->value->reserve(node.total);
                    } else {
                        throw std::runtime_error("Parser Error!!");
                    }
                    state = WaitLFSZ;
                } else if (c >= '0' && c <= '9') {
                    if (auto it = std::get_if<Array>(node.value.getPtr())) {
                        if (node.total == 0) {
                            it->value = std::vector<std::unique_ptr<RespValue>>();
                        }
                        node.total *= 10;
                        node.total += c - '0';
                    } else if (auto it = std::get_if<BulkString>(node.value.getPtr())) {
                        if (node.total == 0) {
                            it->value = std::string();
                        }
                        node.total *= 10;
                        node.total += c - '0';
                    } else {
                        throw std::runtime_error("Parser Error!!");
                    }
                } else {
                    throw std::runtime_error("Parser Error!!");
                }
                break;
            case WaitLFSZ:
                if (c == '\n') {
                    if (auto it = std::get_if<Array>(node.value.getPtr())) {
                        push(std::move(node));
                        state = WaitType;
                    } else if (auto it = std::get_if<BulkString>(node.value.getPtr())) {
                        state = ReadBulk;
                    } else {
                        throw std::runtime_error("Parser Error!!");
                    }
                } else {
                    throw std::runtime_error("Parser Error!!");
                }
                break;
            case WaitNil:
                if (c == '1') {
                    state = WaitNilCR;
                } else {
                    throw std::runtime_error("Parser Error!!");
                }
                break;
            case WaitNilCR:
                if (c == '\r') {
                    state = WaitLF;
                } else {
                    throw std::runtime_error("Parser Error!!");
                }
                break;
            case ReadLine:
                if (c == '\r') {
                    state = WaitLF;
                } else {
                    if (auto it = std::get_if<SimpleString>(node.value.getPtr())) {
                        if (it->value.capacity() == it->value.size()) {
                            it->value.reserve(2 * it->value.capacity());
                        }
                        it->value += c;
                    } else if (auto it = std::get_if<Error>(node.value.getPtr())) {
                        if (it->value.capacity() == it->value.size()) {
                            it->value.reserve(2 * it->value.capacity());
                        }
                        it->value += c;
                    } else if (auto it = std::get_if<int64_t>(node.value.getPtr())) {
                        if (c == '-') {
                            if (!node.total) {
                                node.total = -1;
                            } else {
                                throw std::runtime_error("Parser Error!!");
                            }
                        } else if (c >= '0' && c <= '9') {
                            if (!node.total) {
                                node.total = 1;
                            }
                            *it *= 10;
                            if (node.total == 1) {
                                *it += c - '0';
                            } else {
                                *it -= c - '0';
                            }
                        } else {
                            throw std::runtime_error("Parser Error!!");
                        }
                    } else {
                        throw std::runtime_error("Parser Error!!");
                    }
                }
                break;
            case WaitLF:
                if (c == '\n') {
                    push(std::move(node));
                    state = WaitType;
                } else {
                    throw std::runtime_error("Parser Error!!");
                }
                break;
            case ReadBulk:
                if (auto it = std::get_if<BulkString>(node.value.getPtr())) {
                    if (it->value->size() == node.total) {
                        if (c == '\r') {
                            state = WaitLF;
                        } else {
                            throw std::runtime_error("Parser Error!!");
                        }
                    } else {
                        *it->value += c;
                    }
                } else {
                    throw std::runtime_error("Parser Error!!");
                }
                break;
            }
        }
    }
    void RespParser::reset() {
        std::stack<RespState>().swap(stateStack);
        node = RespState();
        state = WaitType;
    }
    bool RespParser::hasResult() {
        if (stateStack.size() == 1) {
            RespValue &tp = stateStack.top().value;
            size_t tot = stateStack.top().total;
            if (auto it = std::get_if<Array>(tp.getPtr())) {
                return it->value->size() == tot;
            } else {
                return true;
            }
        }
        return false;
    }
    std::optional<RespValue> RespParser::getResult() {
        if (!hasResult()) {
            return std::nullopt;
        }
        auto result = std::move(stateStack.top().value);
        reset();
        return result;
    }

    void RespParser::push(RespState state) {
        if (auto it = std::get_if<Array>(state.value.getPtr())) {
            if (it->value.has_value() && it->value->size() != state.total) {
                stateStack.push(std::move(state));
                return;
            }
        }
        if (stateStack.empty()) {
            stateStack.push(std::move(state));
        } else {
            RespValue &tp = stateStack.top().value;
            size_t tot = stateStack.top().total;
            if (auto it = std::get_if<Array>(tp.getPtr())) {
                it->value->push_back(std::make_unique<RespValue>(std::move(state.value)));
                if (it->value->size() == tot) {
                    RespState temp = std::move(stateStack.top());
                    stateStack.pop();
                    push(std::move(temp));
                }
            } else {
                throw std::runtime_error("Parser Stack Error!!");
            }
        }
    }
};