// RespEncoder.cpp
#include "RespEncoder.h"
namespace resp {
    void encodeSimpleString(SimpleString msg, std::string &str) {
        str += "+";
        str += msg.value;
        str += "\r\n";
    }
    void encodeError(Error msg, std::string &str) {
        str += "-";
        str += msg.value;
        str += "\r\n";
    }
    void encodeInteger(int64_t num, std::string &str) {
        str += ":";
        str += std::to_string(num);
        str += "\r\n";
    }
    void encodeBulkString(BulkString data, std::string &str) {
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
    void encodeArray(Array elements, std::string &str) {
        if (!elements.value.has_value()) {
            str += "*-1\r\n";
        } else {
            str += std::to_string(elements.value->size());
            str += "\r\n";
            for (const auto &element : *(elements.value)) {
                if (auto it = std::get_if<SimpleString>(&element.get())) {
                    encodeSimpleString(*it, str);
                } else if (auto it = std::get_if<Error>(&element.get())) {
                    encodeError(*it, str);
                } else if (auto it = std::get_if<int64_t>(&element.get())) {
                    encodeInteger(*it, str);
                } else if (auto it = std::get_if<BulkString>(&element.get())) {
                    encodeBulkString(*it, str);
                } else if (auto it = std::get_if<Array>(&element.get())) {
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