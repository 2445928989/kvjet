// RespValue.h
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>
// 递归定义RESP协议传输对象

// 不传参默认构造一个空Array代表nil
// RAII禁用拷贝
namespace resp {
    class RespValue;

    // 简单字符串
    struct SimpleString {
        std::string value;
        SimpleString() {}
        SimpleString(std::string&& s) : value(std::move(s)) {}
    };
    // 原始字符串，nullopt代表nil
    struct BulkString {
        std::optional<std::string> value;
        BulkString() {}
        BulkString(std::string&& s) : value(std::move(s)) {}
        BulkString(std::nullopt_t) : value(std::nullopt) {}
    };
    // 错误信息
    struct Error {
        std::string value;
        Error() {}
        Error(std::string&& s) : value(std::move(s)) {}
    };
    // 数组，nullopt代表nil
    struct Array {
        std::optional<std::vector<std::unique_ptr<RespValue>>> value;
        // 禁止拷贝
        Array() = default;
        Array(const Array &) = delete;
        Array &operator=(const Array &) = delete;
        Array(Array &&) = default;
        Array &operator=(Array &&) = default;
    };
    using Value = std::variant<
        SimpleString,
        BulkString,
        Error,
        int64_t,
        Array>;
    class RespValue {
    public:
        RespValue(SimpleString &&s);

        RespValue(BulkString &&s);

        RespValue(Error &&s);

        RespValue(int64_t i);

        RespValue(Array &&v);

        RespValue();

        // 禁止拷贝
        RespValue(const RespValue &) = delete;
        RespValue &operator=(const RespValue &) = delete;
        RespValue(RespValue &&) = default;
        RespValue &operator=(RespValue &&) = default;

        const Value &get() const;

        Value *getPtr() { return &value; }

        const Value *getPtr() const { return &value; }

    private:
        Value value;
    };
};