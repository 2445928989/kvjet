# KVJet - 分布式内存键值存储系统

一个高性能的分布式键值存储系统，采用现代 C++20 开发。单机 pipeline 模式达到 **73w QPS**，P99 延迟仅 **0.12ms**，支持线性扩展的分布式集群部署。

## 核心特性

- **高吞吐低延迟**：单机 73w QPS，P99 0.12ms
- **RESP 协议**：基于 RESP 协议，自定义扩展传输层
- **分布式设计**：Gossip 协议实现集群拓扑管理，支持节点动态加入/离开
- **数据一致性**：一致性哈希分片，AOF + Snapshot 双层持久化
- **内存管理**：LRU 淘汰策略，多线程并发访问
- **事件驱动**：Reactor 模式 + epoll + eventfd 唤醒机制

## 快速开始

### 编译

```bash
cd /data/kvjet
mkdir build && cd build
cmake ..
make
```

### 启动单机服务器

```bash
./kvserver 127.0.0.1 6379
```

### 搭建分布式集群

终端 1（主节点）：
```bash
./kvserver 192.168.100.1 6379
```

终端 2（从节点）：
```bash
./kvserver 192.168.100.2 6380 --join 192.168.100.1 6379
```

### 运行基准测试

```bash
# 吞吐量测试
./kvclient 127.0.0.1 6379 --benchmark 100000 mixed --threads 16

# 延迟测试
./kvclient 127.0.0.1 6379 --latency 100000 mixed
```

## 系统架构

### 核心模块

- **Server**: 基于 epoll + eventfd 的 Reactor 事件循环，管理客户端连接和数据一致性
- **KVStore**: 哈希表存储引擎，集成 LRU 淘汰、过期管理、AOF 日志和 Snapshot 快照
- **Cluster**: Gossip 协议实现集群拓扑管理，一致性哈希实现数据分片路由
- **Client**: 客户端和基准测试工具，支持 benchmark 和 latency 测试
- **RESP**: RESP 协议编码器、解码器，处理网络通信

### 网络协议

- RESP（Redis 序列化协议）基础格式，并进行了定制扩展
- 自定义 Gossip 消息：NODEIN/NODEOUT 集群协调

## 性能数据

| 指标 | 数值 |
|------|------|
| 单机 QPS（Pipeline） | 73w |
| 单机 P99 延迟 | 0.12ms |
| 集群扩展 | 线性 |

## 技术亮点

1. **事件驱动优化**：用 eventfd 替代轮询，消除忙等 CPU 占用，单机 QPS 显著提升
2. **集群协调机制**：Gossip 协议 + UUID 去重缓存，确保拓扑信息可靠传播
3. **O(1) LRU 缓存淘汰**：基于双哈希表实现，内存超限时自动清理冷数据
4. **一致性哈希分片**：虚拟节点均衡，节点变更仅迁移影响范围内的数据
5. **双层持久化**：AOF 增量日志 + Snapshot 快照，快速恢复和故障转移
6. **线程池并发**：工作窃取设计，支持 CPU 亲和性绑定

## 项目结构

```
kvjet/
├── src/
│   ├── server/        # 服务器核心 & 事件循环
│   ├── client/        # 客户端 & 基准测试工具
│   ├── kvstore/       # KV 存储引擎 & 持久化
│   ├── util/          # 网络通信、线程管理、集群协调
│   └── resp/          # RESP 协议编码器、解码器
├── CMakeLists.txt
└── README.md
```

## 系统要求

- C++20 编译器（GCC 10+、Clang 12+）
- Linux 系统（需要 epoll 支持）
- CMake 3.15+
- jemalloc

## 许可证

MIT