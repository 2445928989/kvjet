// Utils.cpp
#include "Utils.h"
#include <stdexcept>

uint64_t Utils::to_uint64_t(const std::string &str) {
    uint64_t ret = 0;
    for (auto &c : str) {
        ret *= 10;
        ret += c - '0';
    }
    return ret;
}

Cluster::Node Utils::getNode(resp::RespValue &&value) {
    auto node_ = std::get_if<resp::Array>(value.getPtr());
    if (!node_ || !node_->value || node_->value->size() != 3) {
        throw std::runtime_error("getNode: Invalid node format, expected Array of 3 elements");
    }

    Cluster::Node node;
    auto ip_ = std::get_if<resp::SimpleString>(node_->value.value()[0]->getPtr());
    auto port_ = std::get_if<int64_t>(node_->value.value()[1]->getPtr());
    auto UUID_ = std::get_if<resp::SimpleString>(node_->value.value()[2]->getPtr());

    if (!ip_ || !port_ || !UUID_) {
        throw std::runtime_error("getNode: Invalid node element types");
    }

    node.ip = std::move(ip_->value);
    node.port = static_cast<uint16_t>(*port_);
    node.UUID = to_uint64_t(UUID_->value);

    return node;
}

std::vector<Cluster::Node> Utils::getTopo(resp::RespValue &&value) {
    std::vector<Cluster::Node> topo;
    auto topo_ = std::get_if<resp::Array>(value.getPtr());
    if (!topo_ || !topo_->value) {
        throw std::runtime_error("getTopo: Invalid topo format, expected Array");
    }

    for (const auto &node_ptr : *topo_->value) {
        topo.push_back(getNode(std::move(*node_ptr)));
    }

    return topo;
}

std::vector<Cluster::Node> Utils::getTopoV2(resp::RespValue &&value,
        std::map<uint64_t, std::vector<uint64_t>> &groups) {
    auto outer = std::get_if<resp::Array>(value.getPtr());
    if (!outer || !outer->value || outer->value->size() < 1)
        throw std::runtime_error("getTopoV2: Invalid format");

    // 第一部分：节点列表
    auto nodes = getTopo(std::move(*(*outer->value)[0]));

    // 第二部分：组信息（可能不存在 — 兼容旧格式）
    if (outer->value->size() >= 2) {
        auto grp_arr = std::get_if<resp::Array>((*outer->value)[1]->getPtr());
        if (grp_arr && grp_arr->value) {
            for (auto &gptr : *grp_arr->value) {
                auto ga = std::get_if<resp::Array>(gptr->getPtr());
                if (!ga || !ga->value || ga->value->size() < 2) continue;
                uint64_t gid = Utils::to_uint64_t(
                    std::get_if<resp::SimpleString>((*ga->value)[0]->getPtr())->value);
                std::vector<uint64_t> members;
                for (size_t i = 1; i < ga->value->size(); i++) {
                    auto s = std::get_if<resp::SimpleString>((*ga->value)[i]->getPtr());
                    if (s) {
                        uint64_t uid = Utils::to_uint64_t(s->value);
                        if (uid != 0) members.push_back(uid);
                    }
                }
                groups[gid] = std::move(members);
            }
        }
    }

    return nodes;
}