// RespValue.h
#pragma once
#include <optional>
#include <string>
#include <variant>
#include <vector>
// 递归定义RESP协议传输对象
namespace resp {
    class RespValue;

    // 简单字符串
    struct SimpleString {
        std::string value;
    };
    // 原始字符串，nullopt代表nil
    struct BulkString {
        std::optional<std::string> value;
    };
    // 错误信息
    struct Error {
        std::string value;
    };
    // 数组，nullopt代表nil
    struct Array {
        std::optional<std::vector<RespValue>> value;
    };
    using Value = std::variant<
        SimpleString,
        BulkString,
        Error,
        int64_t,
        Array>;
    class RespValue {
    public:
        RespValue(SimpleString s);

        RespValue(BulkString s);

        RespValue(Error s);

        RespValue(int64_t i);

        RespValue(Array v);

        Value get();

        const Value &get() const;

    private:
        Value value;
    };
};