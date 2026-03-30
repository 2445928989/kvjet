// RespEncoder.h
#pragma once
#include "RespValue.h"
#include <string>
#include <vector>
// 用于将RespValue对象序列化成string
// 使用时直接调用encode方法
namespace resp {
    void encodeArray(const Array &elements, std::string &str);
    void encodeSimpleString(const SimpleString &msg, std::string &str);
    void encodeBulkString(const BulkString &data, std::string &str);
    void encodeError(const Error &msg, std::string &str);
    void encodeInteger(int64_t num, std::string &str);
    // 编码RespValue对象
    std::string encode(const RespValue &val);
};