// RespEncoder.h
#pragma once
#include "RespValue.h"
#include <string>
#include <vector>
namespace resp {
    void encodeSimpleString(SimpleString msg, std::string &str);
    void encodeError(Error msg, std::string &str);
    void encodeInteger(int64_t num, std::string &str);
    void encodeBulkString(BulkString data, std::string &str);
    void encodeArray(Array elements, std::string &str);

    std::string encode(const RespValue &val);
};