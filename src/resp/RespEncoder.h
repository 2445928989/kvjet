// RespEncoder.h
#pragma once
#include "../util/Cluster.h"
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
    // 编码网络节点信息
    std::string encodeNode(const Cluster::Node &node);
    // 编码网络拓扑信息
    std::string encodeTopo(const std::map<uint64_t, Cluster::Node> &topo);
};