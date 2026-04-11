// Utils.h
#pragma once
#include "../resp/RespValue.h"
#include "../util/Cluster.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Utils {
    uint64_t to_uint64_t(const std::string &str);

    // 从Array格式的RespValue解包成Node对象
    Cluster::Node getNode(resp::RespValue &&value);

    // 从Array格式的RespValue解包成vector<Node>
    std::vector<Cluster::Node> getTopo(resp::RespValue &&value);
}