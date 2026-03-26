// RespValue.cpp

#include "RespValue.h"

using namespace resp;

RespValue::RespValue(std::string s) : value(std::move(s)) {}

RespValue::RespValue(int64_t i) : value(i) {}

RespValue::RespValue(std::vector<RespValue> v) : value(std::move(v)) {}

RespValue::RespValue() : value(nullptr) {}

Value RespValue::get() {
    return value;
}
