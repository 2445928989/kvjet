// Cluster.h
#pragma once
#include <chrono>
#include <cstdint>

class Cluster {
public:
    struct Heartbeat {
        uint32_t node_id;
        uint64_t timestamp;
        Heartbeat(uint32_t node_id) : node_id(node_id), timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) {}
    };
private:
    
};