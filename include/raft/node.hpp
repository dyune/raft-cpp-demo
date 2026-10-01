#pragma once

#include "raft/config.hpp"
#include "raft/kv_store.hpp"
#include "raft/transport.hpp"
#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace raft {
struct NodeSnapshot {
    NodeId id{}; Term term{}; Role role{Role::follower}; bool running{};
    LogIndex log_size{}; LogIndex commit_index{}; LogIndex last_applied{};
};

class Node {
public:
    Node(NodeId id, std::vector<NodeId> peers, Transport& transport,
         const Config& config);
    ~Node();
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    void start();
    void crash();
    void receive(Envelope envelope);
    [[nodiscard]] NodeSnapshot snapshot() const;
    [[nodiscard]] std::optional<std::string> get(const std::string& key) const;
    bool submit(Command command, std::chrono::milliseconds timeout);
private:
    void run();
    void reset_election_deadline();
    void begin_election();
    void become_follower(Term term);
    void become_leader();
    void send_heartbeats();
    void send_append(NodeId peer);
    void handle(NodeId from, const RequestVote& message);
    void handle(NodeId from, const RequestVoteReply& message);
    void handle(NodeId from, const AppendEntries& message);
    void handle(NodeId from, const AppendEntriesReply& message);
    void advance_commit();
    void apply_committed();
    [[nodiscard]] bool has_majority(std::size_t count) const noexcept;
    void log_event(const std::string& event) const;
    Term term_at(LogIndex index) const;

    const NodeId id_;
    const std::vector<NodeId> peers_;
    Transport& transport_;
    const Config config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable commit_cv_;
    std::deque<Envelope> inbox_;
    std::thread thread_;
    bool running_{false};
    bool stop_requested_{false};
    Role role_{Role::follower};
    Term current_term_{0};
    std::optional<NodeId> voted_for_;
    std::vector<LogEntry> log_{{}}; // sentinel at index 0
    LogIndex commit_index_{0};
    LogIndex last_applied_{0};
    KvStore store_;
    std::set<NodeId> votes_received_;
    std::vector<LogIndex> next_index_;
    std::vector<LogIndex> match_index_;
    std::mt19937_64 random_;
    std::chrono::steady_clock::time_point election_deadline_;
    std::chrono::steady_clock::time_point heartbeat_deadline_;
};
} // namespace raft
