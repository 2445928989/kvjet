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