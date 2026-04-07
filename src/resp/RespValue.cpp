// RespValue.cpp

#include "RespValue.h"
using namespace resp;
namespace resp {

    RespValue::RespValue(SimpleString &&s) : value(std::move(s)) {}

    RespValue::RespValue(BulkString &&s) : value(std::move(s)) {}

    RespValue::RespValue(Error &&s) : value(std::move(s)) {}

    RespValue::RespValue(int64_t i) : value(i) {}

    RespValue::RespValue(Array &&v) : value(std::move(v)) {}

    RespValue::RespValue() : value(Array{}) {}

    const Value &RespValue::get() const {
        return value;
    }

}