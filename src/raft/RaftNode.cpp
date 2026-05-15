// RaftNode.cpp
#include "RaftNode.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

// ============ RESP 编解码 ============
// AppendEntries:     *5\r\n+RAFT_AE\r\n+gid\r\n+term\r\n+prevIdx\r\n+prevTerm\r\n
//                    +leaderCommit\r\n
//                    若有日志: +cnt\r\n+cmd1\r\n+cmd2...
// AppendEntriesReply:*4\r\n+RAFT_AER\r\n+gid\r\n+term\r\n+success\r\n+matchIdx\r\n
// RequestVote:       *5\r\n+RAFT_RV\r\n+gid\r\n+term\r\n+lastIdx\r\n+lastTerm\r\n
// RequestVoteReply:  *4\r\n+RAFT_RVR\r\n+gid\r\n+term\r\n+voteGranted\r\n

static std::string encodeAppendEntries(uint64_t gid, uint64_t term,
                                        uint64_t prevIdx, uint64_t prevTerm,
                                        uint64_t leaderCommit,
                                        const std::vector<LogEntry> &entries) {
    std::string s = "*6\r\n+RAFT_AE\r\n+" + std::to_string(gid) + "\r\n+" +
                    std::to_string(term) + "\r\n+" +
                    std::to_string(prevIdx) + "\r\n+" +
                    std::to_string(prevTerm) + "\r\n+" +
                    std::to_string(leaderCommit) + "\r\n";
    if (!entries.empty()) {
        s += "+" + std::to_string(entries.size()) + "\r\n";
        for (auto &e : entries)
            s += "+" + e.command + "\r\n";
    } else {
        s += "+0\r\n";
    }
    return s;
}

static std::string encodeAppendEntriesReply(uint64_t gid, uint64_t term,
                                              bool success, uint64_t matchIdx) {
    return "*4\r\n+RAFT_AER\r\n+" + std::to_string(gid) + "\r\n+" +
           std::to_string(term) + "\r\n+" + (success ? "+1\r\n" : "+0\r\n") +
           "+" + std::to_string(matchIdx) + "\r\n";
}

static std::string encodeRequestVote(uint64_t gid, uint64_t term,
                                      uint64_t lastIdx, uint64_t lastTerm) {
    return "*5\r\n+RAFT_RV\r\n+" + std::to_string(gid) + "\r\n+" +
           std::to_string(term) + "\r\n+" + std::to_string(lastIdx) +
           "\r\n+" + std::to_string(lastTerm) + "\r\n";
}

static std::string encodeRequestVoteReply(uint64_t gid, uint64_t term,
                                            bool granted) {
    return "*4\r\n+RAFT_RVR\r\n+" + std::to_string(gid) + "\r\n+" +
           std::to_string(term) + "\r\n+" + (granted ? "+1\r\n" : "+0\r\n");
}

// ============ 解析辅助 ============

static std::vector<std::string> parseParts(const std::string &raw) {
    std::vector<std::string> parts;
    size_t pos = 0, start = 0;
    while ((pos = raw.find('\r', start)) != std::string::npos) {
        std::string chunk = raw.substr(start, pos - start);
        if (!chunk.empty() && chunk[0] == '+')
            parts.push_back(chunk.substr(1));
        start = pos + 2;
    }
    return parts;
}

// ============ RaftNode ============

RaftNode::RaftNode(uint64_t group_id, uint64_t self_id,
                   const std::vector<uint64_t> &peers, SendCb cb,
                   const std::string &log_dir)
    : groupId_(group_id), selfId(self_id), peers(peers), sendCb(std::move(cb)),
      logPath_(log_dir + "group_" + std::to_string(group_id) + ".log") {
    log.push_back({0, ""});  // 哨兵
    std::filesystem::create_directories(log_dir);
    loadFromDisk();
    if (peers.empty()) {
        becomeLeader();
    } else {
        resetElectionTimer();
    }
}

void RaftNode::resetElectionTimer() {
    electionTimer = ELECTION_TIMEOUT_MIN +
                    (std::rand() % (ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN + 1));
}

void RaftNode::becomeFollower(uint64_t term) {
    state_ = Follower;
    currentTerm = term;
    votedFor = std::nullopt;
    resetElectionTimer();
    voteCount = 0;
}

void RaftNode::becomeCandidate() {
    state_ = Candidate;
    currentTerm++;
    votedFor = selfId;
    voteCount = 1;
    resetElectionTimer();
}

void RaftNode::becomeLeader() {
    state_ = Leader;
    nextIndex.assign(peers.size(), lastLogIndex() + 1);
    matchIndex.assign(peers.size(), 0);
    voteCount = 0;
    heartbeatTimer = 0;
}

// ============ tick ============

void RaftNode::tick() {
    if (state_ == Leader) {
        heartbeatTimer--;
        if (heartbeatTimer <= 0) {
            heartbeatTimer = HEARTBEAT_INTERVAL;
            for (size_t i = 0; i < peers.size(); i++)
                sendAppendEntries(peers[i]);
        }
        return;
    }

    electionTimer--;
    if (electionTimer <= 0) {
        becomeCandidate();
        sendRequestVote();
    }
}

// ============ step ============

void RaftNode::step(uint64_t from_peer, const std::string &raw) {
    auto parts = parseParts(raw);
    if (parts.empty()) return;

    if (parts[0] == "RAFT_AE")
        handleAppendEntries(from_peer, raw);
    else if (parts[0] == "RAFT_AER")
        handleAppendEntriesReply(from_peer, raw);
    else if (parts[0] == "RAFT_RV")
        handleRequestVote(from_peer, raw);
    else if (parts[0] == "RAFT_RVR")
        handleRequestVoteReply(from_peer, raw);
}

// ============ AppendEntries ============

void RaftNode::handleAppendEntries(uint64_t from_peer, const std::string &raw) {
    auto parts = parseParts(raw);
    // parts: RAFT_AE, gid, term, prevIdx, prevTerm, leaderCommit, [cnt, cmd1...]
    if (parts.size() < 6) return;

    uint64_t term          = std::stoull(parts[2]);
    uint64_t prevIdx       = std::stoull(parts[3]);
    uint64_t prevTerm      = std::stoull(parts[4]);
    uint64_t leaderCommit  = std::stoull(parts[5]);

    if (term < currentTerm) {
        sendCb({from_peer,
                encodeAppendEntriesReply(groupId_, currentTerm, false, 0)});
        return;
    }

    if (term > currentTerm || state_ == Candidate)
        becomeFollower(term);
    resetElectionTimer();

    // 日志一致性检查
    if (prevIdx > 0 && (prevIdx >= log.size() || log[prevIdx].term != prevTerm)) {
        sendCb({from_peer,
                encodeAppendEntriesReply(groupId_, currentTerm, false, 0)});
        return;
    }

    // 追加条目
    bool truncated = false;
    if (parts.size() > 6) {
        uint64_t cnt = std::stoull(parts[6]);
        for (uint64_t i = 0; i < cnt && (7 + i) < parts.size(); i++) {
            uint64_t idx = prevIdx + 1 + i;
            if (idx < log.size() && log[idx].term != term) {
                log.resize(idx);
                truncated = true;
            }
            if (idx >= log.size()) {
                log.push_back({term, parts[7 + i]});
                appendToDisk(log.back());
            }
        }
    }
    if (truncated)
        rewriteDisk();

    // 更新 commitIndex
    if (leaderCommit > commitIndex)
        commitIndex = std::min(leaderCommit, lastLogIndex());

    sendCb({from_peer,
            encodeAppendEntriesReply(groupId_, currentTerm, true, lastLogIndex())});
}

// ============ AppendEntriesReply ============

void RaftNode::handleAppendEntriesReply(uint64_t from_peer, const std::string &raw) {
    if (state_ != Leader) return;

    auto parts = parseParts(raw);
    if (parts.size() < 4) return;

    uint64_t term    = std::stoull(parts[2]);
    bool     success = (parts[3] == "1");
    uint64_t matchIdx = std::stoull(parts[4]);

    if (term > currentTerm) {
        becomeFollower(term);
        return;
    }

    size_t pi = peerIndex(from_peer);
    if (pi >= peers.size()) return;

    if (success) {
        nextIndex[pi]  = matchIdx + 1;
        matchIndex[pi] = matchIdx;
        advanceCommitIndex();
    } else {
        if (nextIndex[pi] > 1)
            nextIndex[pi]--;
        sendAppendEntries(from_peer);
    }
}

void RaftNode::advanceCommitIndex() {
    // 对 matchIndex 排序，取中位数
    std::vector<uint64_t> sorted = matchIndex;
    sorted.push_back(lastLogIndex());  // 自己也算
    std::sort(sorted.begin(), sorted.end(), std::greater<uint64_t>());
    uint64_t quorum = peers.size() / 2 + 1;
    uint64_t newCommit = sorted[quorum - 1];
    if (newCommit > commitIndex && log[newCommit].term == currentTerm)
        commitIndex = newCommit;
}

// ============ RequestVote ============

void RaftNode::handleRequestVote(uint64_t from_peer, const std::string &raw) {
    auto parts = parseParts(raw);
    if (parts.size() < 5) return;

    uint64_t term         = std::stoull(parts[2]);
    uint64_t candLastIdx  = std::stoull(parts[3]);
    uint64_t candLastTerm = std::stoull(parts[4]);

    if (term < currentTerm) {
        sendCb({from_peer, encodeRequestVoteReply(groupId_, currentTerm, false)});
        return;
    }

    if (term > currentTerm)
        becomeFollower(term);

    bool logOk = (candLastTerm > lastLogTerm()) ||
                 (candLastTerm == lastLogTerm() && candLastIdx >= lastLogIndex());
    bool canVote = !votedFor.has_value() || votedFor.value() == from_peer;

    if (canVote && logOk) {
        votedFor = from_peer;
        resetElectionTimer();
        sendCb({from_peer, encodeRequestVoteReply(groupId_, currentTerm, true)});
    } else {
        sendCb({from_peer, encodeRequestVoteReply(groupId_, currentTerm, false)});
    }
}

// ============ RequestVoteReply ============

void RaftNode::handleRequestVoteReply(uint64_t /*from_peer*/, const std::string &raw) {
    if (state_ != Candidate) return;

    auto parts = parseParts(raw);
    if (parts.size() < 4) return;

    uint64_t term    = std::stoull(parts[2]);
    bool     granted = (parts[3] == "1");

    if (term > currentTerm) {
        becomeFollower(term);
        return;
    }

    if (granted) {
        voteCount++;
        size_t quorum = peers.size() / 2 + 1;
        if (static_cast<size_t>(voteCount) >= quorum)
            becomeLeader();
    }
}

// ============ propose ============

std::optional<std::string> RaftNode::propose(const std::string &cmd) {
    if (state_ != Leader)
        return std::nullopt;

    log.push_back({currentTerm, cmd});
    appendToDisk(log.back());
    for (auto &peer : peers)
        sendAppendEntries(peer);

    if (peers.empty())
        commitIndex = lastLogIndex();

    return std::nullopt;
}

// ============ 消息发送 ============

void RaftNode::sendAppendEntries(uint64_t peer) {
    size_t pi = peerIndex(peer);
    if (pi >= peers.size()) return;

    uint64_t prevIdx  = nextIndex[pi] - 1;
    uint64_t prevTerm = (prevIdx < log.size()) ? log[prevIdx].term : 0;

    std::vector<LogEntry> entries;
    for (uint64_t i = nextIndex[pi]; i < log.size(); i++)
        entries.push_back(log[i]);

    sendCb({peer, encodeAppendEntries(groupId_, currentTerm, prevIdx, prevTerm,
                                        commitIndex, entries)});
}

void RaftNode::sendRequestVote() {
    for (auto &peer : peers)
        sendCb({peer, encodeRequestVote(groupId_, currentTerm,
                                          lastLogIndex(), lastLogTerm())});
}

// ============ 辅助 ============

size_t RaftNode::peerIndex(uint64_t uuid) const {
    auto it = std::find(peers.begin(), peers.end(), uuid);
    return static_cast<size_t>(it - peers.begin());
}

uint64_t RaftNode::lastLogIndex() const {
    return log.size() - 1;
}

uint64_t RaftNode::lastLogTerm() const {
    if (log.size() <= 1) return 0;
    return log.back().term;
}

bool RaftNode::hasPendingApply() {
    return commitIndex > lastApplied;
}

std::vector<LogEntry> RaftNode::takePendingApply() {
    std::vector<LogEntry> result;
    while (lastApplied < commitIndex) {
        lastApplied++;
        if (lastApplied < log.size())
            result.push_back(log[lastApplied]);
    }
    return result;
}

// ============ 日志持久化 ============

void RaftNode::appendToDisk(const LogEntry &e) {
    std::ofstream of(logPath_, std::ios::app | std::ios::binary);
    of << e.term << ' ' << e.command << '\n';
}

void RaftNode::rewriteDisk() {
    std::ofstream of(logPath_, std::ios::trunc | std::ios::binary);
    for (size_t i = 1; i < log.size(); i++)  // 跳过哨兵 idx=0
        of << log[i].term << ' ' << log[i].command << '\n';
}

void RaftNode::loadFromDisk() {
    std::ifstream inf(logPath_, std::ios::binary);
    if (!inf.is_open()) return;
    std::string line;
    while (std::getline(inf, line)) {
        if (line.empty()) continue;
        auto sp = line.find(' ');
        if (sp == std::string::npos) continue;
        uint64_t term = std::stoull(line.substr(0, sp));
        std::string cmd = line.substr(sp + 1);
        log.push_back({term, std::move(cmd)});
    }
    // 恢复哨兵
    if (log.empty() || log[0].term != 0)
        log.insert(log.begin(), {0, ""});
    commitIndex = lastLogIndex();
    lastApplied = lastLogIndex();
    std::cout << "[Raft] Loaded " << (log.size() - 1)
              << " entries from " << logPath_ << std::endl;
}
