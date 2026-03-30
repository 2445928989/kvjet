// RespEncoder.cpp
#include "RespEncoder.h"
namespace resp {
    void encodeSimpleString(const SimpleString& msg, std::string &str) {
        str += "+";
        str += msg.value;
        str += "\r\n";
    }
    void encodeError(const Error& msg, std::string &str) {
        str += "-";
        str += msg.value;
        str += "\r\n";
    }
    void encodeInteger(int64_t num, std::string &str) {
        str += ":";
        str += std::to_string(num);
        str += "\r\n";
    }
    void encodeBulkString(const BulkString& data, std::string &str) {
        str += "$";
        if (!data.value.has_value()) {
            str += "-1\r\n";
        } else {
            str += std::to_string(data.value->size());
            str += "\r\n";
            str += *(data.value);
            str += "\r\n";
        }
    }
    void encodeArray(const Array &elements, std::string &str) {
        if (!elements.value.has_value()) {
            str += "*-1\r\n";
        } else {
            str += "*";
            str += std::to_string(elements.value->size());
            str += "\r\n";
            for (const auto &ptr : *(elements.value)) {
                if (auto it = std::get_if<SimpleString>(ptr->getPtr())) {
                    encodeSimpleString(*it, str);
                } else if (auto it = std::get_if<Error>(ptr->getPtr())) {
                    encodeError(*it, str);
                } else if (auto it = std::get_if<int64_t>(ptr->getPtr())) {
                    encodeInteger(*it, str);
                } else if (auto it = std::get_if<BulkString>(ptr->getPtr())) {
                    encodeBulkString(*it, str);
                } else if (auto it = std::get_if<Array>(ptr->getPtr())) {
                    encodeArray(*it, str);
                }
            }
        }
    }

    std::string encode(const RespValue &val) {
        std::string ret;
        if (auto it = std::get_if<SimpleString>(&val.get())) {
            encodeSimpleString(*it, ret);
        } else if (auto it = std::get_if<Error>(&val.get())) {
            encodeError(*it, ret);
        } else if (auto it = std::get_if<int64_t>(&val.get())) {
            encodeInteger(*it, ret);
        } else if (auto it = std::get_if<BulkString>(&val.get())) {
            encodeBulkString(*it, ret);
        } else if (auto it = std::get_if<Array>(&val.get())) {
            encodeArray(*it, ret);
        }
        return ret;
    }
};