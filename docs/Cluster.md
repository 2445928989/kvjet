# KVJet 集群架构

- **上层**：一致性哈希环将 key 映射到某个 Raft 组（通过 lower_bound）
- **下层**：每个 Raft 组是 3 个节点的复制组，组内通过 Raft 协议保证一致性

## 节点多归属

每个物理节点同时扮演多个角色：

```
节点 0: [PD participant] [组0: master]  [组 N-2: replica] [组 N-1: replica]
节点 1: [PD participant] [组0: replica] [组1: master]     [组 N-1: replica]
节点 2: [PD participant] [组0: replica] [组1: replica]    [组2: master]
...
```

Raft 组数量 = 节点数量。组 i 的成员 = {节点 i, 节点 i+1, 节点 i+2} mod N。每个节点恰好参与 3 个组（1 个 master + 2 个 replica，或者 3 个 replica）。

## 线程模型

只开两个线程：

```
主线程 (epoll):   所有 IO、命令处理、Raft tick、Raft 消息收发、PD 消息收发
一堆心跳线程:         节点级心跳 → 告诉 PD "我还活着"（每 1s）
```

- Raft 的 AppendEntries（组内 50ms）由主线程 tick 驱动
- PD 的所有消息走主线程的消息队列，和普通命令同一通道
- 不另开 Raft 线程、不另开 PD 端口

## 服务端行为

### 接入集群

```
新节点 Y 启动，带 --join <任意已知节点X的地址>:

  1. Y 连接 X 的主端口，发送 HELLO <Y的IP> <Y的PORT> <Y的UUID>

  2. X 收到 HELLO:
     - 若 X 是 PD participant → 消息在本地 PD 处理
     - 若 X 不是 → X 将 HELLO 转发给 PD leader（消息附带 Y 的地址）
     - X 与 Y 之间的这个 socket 暂留，后续看是否在同一组内

  3. PD leader 处理:
     a. 将 Y 加入拓扑表
     b. 计算新的 Raft 组分配（N → N+1，新增一组，末尾几组成员变更）
     c. 主动 connect 回 Y（bootstrap 例外：PD 已从 HELLO 拿到 Y 的地址）
     d. 向 Y 下发配置: "你参与组α(replica)、组β(master)，成员列表是..."
     e. 向所有受影响的已有节点下发配置变更

  4. 收到配置变更的节点（含 Y）各自检查自己所参与的组:
     - 遍历组内成员列表
     - 对每个成员: 若 UUID < 自己 → 主动连对方
     - 对每个成员: 若 UUID > 自己 → 等对方连自己
     - 对方连过来后发 HELLO 互通身份

  5. 与 Y 无关的已有节点不受影响，不建立连接

  6. Y 为每个分配的组创建 RaftNode 实例（初始状态: follower）

  7. Y 从各组的 leader 接收 Raft log / snapshot → 追到最新 → 正常工作
```

### 作为 follower

```
写路径:
  1. 收到客户端 SET/DEL → 返回 MOVED（指向 master 地址）
     或：转发请求给 master

读路径:
  1. 收到客户端 GET → 读本地 KVStore → 返回
     （可能返回稍旧数据，缓存场景可接受）

Raft 消息处理:
  1. 收到 leader 的 AppendEntries:
     - 检查 term，比自己旧则拒绝
     - 检查日志一致性（prevIdx / prevTerm）
     - 追加新日志条目 → 回复 ACK
     - 更新 commitIndex → apply 到本地 KVStore
  2. 超时未收到 leader 心跳:
     - 发起选举（变为 candidate）
     - 发送 RequestVote 给组内其他节点
  3. 收到 candidate 的 RequestVote:
     - 检查 term 和日志新旧程度
     - 日志至少和自己一样新 → 投票
     - 日志比自己旧 → 拒绝
```

### 作为 master

```
写路径:
  1. 收到客户端 SET/DEL
  2. 追加命令到 Raft log
  3. 并行发送 AppendEntries 给所有 follower
  4. 收到多数确认（≥ quorum）→ commit
  5. apply 到本地 KVStore → 返回 OK

读路径:
  1. 收到客户端 GET
  2. 直接读本地 KVStore → 返回

心跳:
  1. 定期（每 50ms）发送 AppendEntries（不带日志条目时即为心跳）
  2. 保持 leader 权威，阻止 follower 超时选举

任期维护:
  1. 收到任何 term > currentTerm 的消息 → 立即退位为 follower
```

### 作为 PD

```
PD 自身:
  - PD 是一个独立的 Raft 组（3-5 个节点），PD 参与者由集群初始节点担任
  - 维护元数据：全局拓扑表、Raft 组分配表、一致性哈希环信息
  - 所有 PD 决策通过自身 Raft log 达成一致

调度职责:
  - 新节点加入 → 创建新 Raft 组，分配成员角色，下发配置
  - 节点心跳超时 → 标记节点离线，补 replica 角色，通知相关组触发选举
  - 管理员 DECOMMISSION → 迁移角色，安全移除节点
  - 负载均衡 → 定期检查各节点负载，必要时迁移 group 的 master 角色

心跳检测:
  - 每个节点每 1s 向 PD 发送心跳
  - PD 3s 未收到 → 判定节点离线
  - 与 Raft 组的 AppendEntries 心跳互不干扰（不同层）
```

### 离开集群

```
正常离开（DECOMMISSION 命令）:
  1. 管理员向 PD 发送 DECOMMISSION <node_uuid>
  2. PD 标记该节点为 "退役中"
  3. 迁移该节点上所有 master 角色:
     - 通知对应的 Raft 组，group 内 replica 发起选举
     - 新 leader 选出后，继续服务
  4. 迁移该节点上所有 replica 角色:
     - PD 将空出的 replica 位置分配给负载最低的其他节点
     - 新 replica 从 leader 全量同步数据
  5. 该节点所有角色清空 → 安全关闭

异常离开（crash / 网络分区）:
  1. PD 心跳超时（3s）→ 判定节点离线
  2. PD 补位:
     - 其 master 角色 → Raft 组内自动选举新 leader
     - 其 replica 角色 → PD 分配给其他节点
  3. 离线节点恢复后:
     - 重新接入集群（走接入流程）
     - 以新身份（全新的 follower / replica）加入
     - 旧数据从 leader 全量同步

快照和恢复:
  1. 正常离开时，节点将本地 KVStore 数据写入 snapshot
  2. 异常离开时，数据在组内其他节点有 Raft log + 副本，不丢
```

## 相关命令

```
HELLO <IP> <PORT> <UUID>
  告诉 PD "我来了"。PD 返回配置（哪些组、各是什么角色）

GETNETWORK
  获取完整网络拓扑（含 Raft 组分配）

DECOMMISSION <UUID>
  管理员命令，安全下线节点

MOVED <target_addr>
  数据已迁移，请重定向到目标节点

RAFT_AE <group_id> <term> <prevIdx> <prevTerm> [cnt] [cmds...]
  Raft AppendEntries

RAFT_AER <group_id> <term> <success> <matchIdx>
  Raft AppendEntries 响应

RAFT_RV <group_id> <term> <lastIdx> <lastTerm>
  Raft RequestVote

RAFT_RVR <group_id> <term> <voteGranted>
  Raft RequestVote 响应
```

## Raft 组分配算法

```
N 个节点 → N 个 Raft 组，每组 3 个节点

组 i 的成员 = {节点 (i mod N), 节点 ((i+1) mod N), 节点 ((i+2) mod N)}
组 i 的 master = 节点 (i mod N)

示例（4 节点）:
  组0: [节点0(master), 节点1, 节点2]
  组1: [节点1(master), 节点2, 节点3]
  组2: [节点2(master), 节点3, 节点0]
  组3: [节点3(master), 节点0, 节点1]

每个节点恰好出现在 3 个组中，1 次 master + 2 次 replica。
```
