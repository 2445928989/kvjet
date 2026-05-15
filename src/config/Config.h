// Config.h
#pragma once
#include <cstdint>
#include <string>
// 用于修改配置常量
namespace Config {
    // ========== Server配置 ==========
    // epoll最大事件数
    constexpr int MAX_EVENTS = 1024;

    // 单次recv最大接收字节数
    constexpr size_t MAX_RECV_SIZE = 1024 * 1024; // 1MB

    // 读写线程数
    constexpr int THREAD_COUNT = 4; // 默认4线程

    // ========== KVStore配置 ==========
    // 分片数
    constexpr size_t SHARD_COUNT = 16;

    // 每分片LRU最大容量
    constexpr int LRU_MAX_SIZE = 100000;

    // ========== AOF配置 ==========
    // AOF缓冲区大小，超过此大小自动flush
    constexpr size_t AOF_BUFFER_SIZE = 1024 * 1024; // 1MB

    // AOF定时flush间隔（毫秒）
    constexpr int AOF_FSYNC_TIME_MS = 1000; // 1秒

    // AOF保存目录
    constexpr std::string AOF_DIR = "data/aof.dat";

    // ========== 快照配置 ==========
    // RDB快照保存间隔（毫秒）
    constexpr int SNAPSHOT_INTERVAL_MS = 10000; // 10秒

    // 快照保存目录
    constexpr std::string SNAPSHOT_DIR = "data/kvstore/";

    // ========== 集群配置 ==========
    // 一致性哈希中每个物理节点的虚拟节点数
    constexpr size_t VIRTUAL_REPLICAS = 160;

    // ========== 网络配置 ==========
    // 服务器监听端口
    constexpr uint16_t SERVER_PORT = 7800;

    // 心跳线程监听端口
    constexpr uint16_t HEARTBEAT_PORT = 7801;

    // gossip缓存最大容量
    constexpr size_t GOSSIP_CACHE_SIZE = 1000;

    // gossip超时时间（毫秒）
    constexpr int GOSSIP_TIMEOUT_MS = 60000; // 60秒

    // 心跳发送间隔（毫秒）
    constexpr int HEARTBEAT_INTERVAL_MS = 1000; // 1秒

    // 心跳超时判定（毫秒）
    constexpr int HEARTBEAT_TIMEOUT_MS = 3000; // 3秒

    // ========== Raft 配置 ==========
    // Raft 日志持久化目录
    constexpr std::string RAFT_LOG_DIR = "data/raft/";
}
