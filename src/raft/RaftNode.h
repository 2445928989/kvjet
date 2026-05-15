// RaftNode.h — 单 Raft 组状态机
#pragma once

#include "../config/Config.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct RaftMessage {
    uint64_t target;            // 目标节点 UUID
    std::string payload;        // RESP 编码后的消息体
};

struct LogEntry {
    uint64_t term;
    std::string command;
};

// RaftNode 管理一个 Raft 组的状态。
// tick(from_peer, raw) — 推进选举计时和心跳
// step(from_peer, raw)  — 处理收到的 Raft 消息
// propose(cmd)           — leader 提交命令
class RaftNode {
public:
    using SendCb = std::function<void(RaftMessage)>;

    RaftNode(uint64_t group_id, uint64_t self_id,
             const std::vector<uint64_t> &peers, SendCb cb,
             const std::string &log_dir);

    enum State { Follower, Candidate, Leader };
    State       state() const { return state_; }
    uint64_t    term() const { return currentTerm; }
    bool        isLeader() const { return state_ == Leader; }

    void                          tick();
    void                          step(uint64_t from_peer, const std::string &raw);
    std::optional<std::string>    propose(const std::string &cmd);

    bool                          hasPendingApply();
    std::vector<LogEntry>         takePendingApply();

    uint64_t                      groupId() const { return groupId_; }
    const std::vector<uint64_t> & peerIds() const { return peers; }

private:
    void becomeFollower(uint64_t term);
    void becomeCandidate();
    void becomeLeader();
    void resetElectionTimer();
    void advanceCommitIndex();

    void handleAppendEntries(uint64_t from_peer, const std::string &raw);
    void handleAppendEntriesReply(uint64_t from_peer, const std::string &raw);
    void handleRequestVote(uint64_t from_peer, const std::string &raw);
    void handleRequestVoteReply(uint64_t from_peer, const std::string &raw);

    void sendAppendEntries(uint64_t peer);
    void sendRequestVote();

    uint64_t           lastLogIndex() const;
    uint64_t           lastLogTerm() const;
    size_t             peerIndex(uint64_t uuid) const;

    // ==== 日志持久化 ====
    void appendToDisk(const LogEntry &e);
    void rewriteDisk();
    void loadFromDisk();

    static constexpr int ELECTION_TIMEOUT_MIN = 150;
    static constexpr int ELECTION_TIMEOUT_MAX = 300;
    static constexpr int HEARTBEAT_INTERVAL   = 50;

    // 持久状态
    uint64_t                   currentTerm = 0;
    std::optional<uint64_t>    votedFor;
    std::vector<LogEntry>      log;          // log[0] 哨兵

    // 易失状态
    uint64_t                   commitIndex = 0;
    uint64_t                   lastApplied = 0;
    State                      state_ = Follower;
    uint64_t                   groupId_;
    uint64_t                   selfId;
    std::vector<uint64_t>      peers;

    // Leader
    std::vector<uint64_t>      nextIndex;
    std::vector<uint64_t>      matchIndex;

    // Candidate
    int                        voteCount = 0;

    // 计时器 (ms)
    int electionTimer  = 0;
    int heartbeatTimer = 0;

    SendCb sendCb;

    // 持久化
    std::string logPath_;
};
