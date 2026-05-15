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
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(Config::HEARTBEAT_INTERVAL_MS));
        if (!running)
            break;
        sendHeartbeats();
        checkTimeouts();
    }
}

void Cluster::sendHeartbeats() {
    std::shared_lock lock(connections_lock);
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    for (auto &[uuid, fd] : connections) {
        if (fd == -1 || !send_cb)
            continue;
        // 同时也更新自己的心跳时间戳（自身永远存活）
        {
            std::unique_lock hb_lock(heartbeat_time_lock);
            heartbeat_time[self_node.UUID] = now;
        }
        send_cb("*2\r\n+HEARTBEAT\r\n+" + std::to_string(self_node.UUID) + "\r\n", fd);
    }
}

void Cluster::checkTimeouts() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    std::vector<uint64_t> dead;
    {
        std::shared_lock lock(heartbeat_time_lock);
        for (auto &[uuid, ts] : heartbeat_time) {
            if (uuid == self_node.UUID)
                continue;
            if (now - ts > Config::HEARTBEAT_TIMEOUT_MS) {
                dead.push_back(uuid);
            }
        }
    }
    for (auto uuid : dead) {
        std::cerr << "[Cluster] Node " << uuid << " timed out, removing."
                  << std::endl;
        delNodeToHash(uuid);
        delTopoNode(uuid);
    }
}

void Cluster::updateHeartbeat(int fd) {
    uint64_t uuid = 0;
    {
        std::shared_lock lock(fd_to_uuid_lock);
        auto it = fd_to_uuid.find(fd);
        if (it == fd_to_uuid.end())
            return;
        uuid = it->second;
    }
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    std::unique_lock lock(heartbeat_time_lock);
    heartbeat_time[uuid] = now;
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

std::vector<uint64_t> Cluster::queryReplicas(const std::string &key, int N) {
    std::shared_lock<std::shared_mutex> lock(node_hash_lock);
    if (node_hash.empty()) return {self_node.UUID};
    auto it = node_hash.lower_bound(hash(key));
    if (it == node_hash.end()) it = node_hash.begin();
    std::vector<uint64_t> result;
    std::set<uint64_t> seen;
    while (result.size() < static_cast<size_t>(N) && !node_hash.empty()) {
        if (seen.insert(it->second).second)
            result.push_back(it->second);
        ++it;
        if (it == node_hash.end()) it = node_hash.begin();
        if (seen.size() >= node_hash.size()) break;
    }
    return result;
}

bool Cluster::isMasterFor(const std::string &key) {
    return queryNode(key) == self_node.UUID;
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
    delConnection(UUID);
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
    {
        std::unique_lock<std::shared_mutex> lock(connections_lock);
        connections[UUID] = fd;
    }
    std::unique_lock<std::shared_mutex> lock(fd_to_uuid_lock);
    fd_to_uuid[fd] = UUID;
}
uint64_t Cluster::randomNode() const {
    std::shared_lock lock(topo_lock);
    if (topo.empty())
        return 0;
    auto it = topo.begin();
    static thread_local std::mt19937_64 rng(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<size_t> dist(0, topo.size() - 1);
    std::advance(it, dist(rng));
    return it->first;
}

uint64_t Cluster::getUuidByFd(int fd) const {
    std::shared_lock lock(fd_to_uuid_lock);
    auto it = fd_to_uuid.find(fd);
    if (it != fd_to_uuid.end())
        return it->second;
    return 0;
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
    int fd = -1;
    {
        std::shared_lock lock(connections_lock);
        auto it = connections.find(UUID);
        if (it != connections.end())
            fd = it->second;
    }
    if (fd != -1) {
        std::unique_lock lock(fd_to_uuid_lock);
        fd_to_uuid.erase(fd);
    }
    {
        std::unique_lock lock(connections_lock);
        connections.erase(UUID);
    }
}