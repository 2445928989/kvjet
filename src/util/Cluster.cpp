// Cluster.cpp
#include "Cluster.h"
#include "../config/Config.h"
#include <random>
#include <sys/epoll.h>
Cluster::Cluster() {
    self_node.UUID = generateUUID();
    self_node.ip = "127.0.0.1";
    self_node.port = 0;
    heartbeat_listensock = -1;
}

Cluster::Cluster(uint16_t heartbeat_port, const std::string &ip) {
    self_node.UUID = generateUUID();
    self_node.ip = ip;
    self_node.port = heartbeat_port - 1; // heartbeat_port = server_port + 1
    heartbeat_listensock = heartbeat_port;
    // 把自己加入到topo里
    topo[self_node.UUID] = self_node;
}

Cluster::~Cluster() {
    stop();
}

void Cluster::heartbeatLoop() {
    // 备忘：心跳包时间戳直接通过读发过来的确定UUID，如果心跳包不在topo里就直接弃
    while (running) {
        for (auto &[id, fd] : connections) {
        }
    }
}

void Cluster::addNodeToHash(uint64_t node_id) {
    std::unique_lock<std::shared_mutex> lock(node_hash_lock);
    uint64_t hsh = node_id;
    for (size_t i = 0; i < Config::VIRTUAL_REPLICAS; i++) {
        hsh = hash(hsh);
        node_hash[hsh] = node_id;
    }
}

void Cluster::delNodeToHash(uint64_t node_id) {
    std::unique_lock<std::shared_mutex> lock(node_hash_lock);
    uint64_t hsh = node_id;
    for (size_t i = 0; i < Config::VIRTUAL_REPLICAS; i++) {
        hsh = hash(hsh);
        node_hash.erase(hsh);
    }
}

uint64_t Cluster::queryNode(const std::string &str) {
    std::shared_lock<std::shared_mutex> lock(node_hash_lock);
    auto it = node_hash.lower_bound(hash(str));
    if (it == node_hash.end()) {
        return node_hash.begin()->second;
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

bool Cluster::findGossip(uint64_t UUID) {
    std::shared_lock<std::shared_mutex> lock(gossip_cache_lock);
    return gossip_cache.count(UUID);
}

void Cluster::addGossip(uint64_t UUID) {
    std::unique_lock<std::shared_mutex> lock(gossip_cache_lock);
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    gossip_cache[UUID] = now;
    for (auto it = gossip_cache.begin(); it != gossip_cache.end();) {
        if (now - it->second > Config::GOSSIP_TIMEOUT_MS) {
            it = gossip_cache.erase(it);
        } else {
            ++it;
        }
    }
    if (gossip_cache.size() > Config::GOSSIP_CACHE_SIZE) {
        gossip_cache.erase(gossip_cache.begin());
    }
}

void Cluster::delTopoNode(uint64_t UUID) {
    {
        std::unique_lock<std::shared_mutex> lock(connections_lock);
        connections.erase(UUID);
    }
    {
        std::unique_lock<std::shared_mutex> lock(topo_lock);
        topo.erase(UUID);
    }
    {
        std::unique_lock<std::shared_mutex> lock(heartbeat_time_lock);
        heartbeat_time.erase(UUID);
    }
}

void Cluster::addTopoNode(Cluster::Node node) {
    std::unique_lock<std::shared_mutex> lock(topo_lock);
    topo[node.UUID] = node;
}

void Cluster::addConnection(uint64_t UUID, int fd) {
    std::unique_lock<std::shared_mutex> lock(connections_lock);
    connections[UUID] = fd;
}
uint64_t Cluster::generateUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}
int Cluster::getConnection(uint64_t UUID) {
    auto it = connections.find(UUID);
    if (it == connections.end())
        return -1;
    else
        return it->second;
}
void Cluster::delConnection(uint64_t UUID) {
    connections.erase(UUID);
}