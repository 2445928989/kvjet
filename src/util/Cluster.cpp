// Cluster.cpp
#include "Cluster.h"
#include "../config/Config.h"
#include <sys/epoll.h>
Cluster::Cluster() {
}

Cluster::~Cluster() {
    stop();
}

void Cluster::heartbeatLoop() {
    while (running) {
        for (auto &[id, fd] : connections) {
        }
    }
}

void Cluster::addNode(uint64_t node_id) {
    std::lock_guard<std::mutex> lock(node_map_lock);
    uint64_t hsh = node_id;
    for (size_t i = 0; i < Config::VIRTUAL_REPLICAS; i++) {
        hsh = hash(hsh);
        node_map[hsh] = node_id;
    }
}

void Cluster::delNode(uint64_t node_id) {
    std::lock_guard<std::mutex> lock(node_map_lock);
    uint64_t hsh = node_id;
    for (size_t i = 0; i < Config::VIRTUAL_REPLICAS; i++) {
        hsh = hash(hsh);
        node_map.erase(hsh);
    }
}

uint64_t Cluster::queryNode(const std::string &str) {
    std::lock_guard<std::mutex> lock(node_map_lock);
    auto it = node_map.lower_bound(hash(str));
    if (it == node_map.end()) {
        return node_map.begin()->second;
    } else {
        return it->second;
    }
}

uint64_t Cluster::hash(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}
uint64_t Cluster::hash(const std::string &str) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;
    for (unsigned char c : str) {
        hash ^= c;
        hash *= prime;
    }
    return hash;
}

bool Cluster::isAlive(uint64_t node_id) {
    auto it = heartbeat_time.find(node_id);
    if (it == heartbeat_time.end()) {
        return false;
    }
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                   .count();
    return (now - it->second) < Config::HEARTBEAT_TIMEOUT_MS;
}

void Cluster::start() {
    running = true;
    heartbeat_thread = std::thread(&Cluster::heartbeatLoop, this);
}

void Cluster::stop() {
    running = false;
    if (heartbeat_thread.joinable())
        heartbeat_thread.join();
}
void Cluster::heartbeatInit() {
    epoll_fd = epoll_create1(0);
}