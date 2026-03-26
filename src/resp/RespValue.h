// RespValue.h
#pragma once
#include <string>
#include <variant>
#include <vector>

namespace resp {
    class RespValue;

    using Value = std::variant<
        std::string,
        int64_t,
        std::vector<RespValue>,
        nullptr_t>;

    class RespValue {
    public:
        RespValue(std::string s);

        RespValue(int64_t i);

        RespValue(std::vector<RespValue> v);

        RespValue();

        Value get();

    private:
        Value value;
    };
};