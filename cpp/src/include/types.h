#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

using NodeId = int;
using Term = std::uint64_t;
using LogIndex = std::size_t;

enum class Role { follower, candidate, leader };
enum class Op { set, del };

struct Command {
    Op op{Op::set};
    std::string key;
    std::string value;
};

struct LogEntry {
    Term term{};
    Command command;
};

struct RequestVote {
    Term term{};
    NodeId candidate{};
    LogIndex last_log_index{};
    Term last_log_term{};
};

struct RequestVoteReply {
    Term term{};
    bool granted{};
};

struct AppendEntries {
    Term term{};
    NodeId leader{};
    LogIndex prev_log_index{};
    Term prev_log_term{};
    std::vector<LogEntry> entries;
    LogIndex leader_commit{};
};

struct AppendEntriesReply {
    Term term{};
    bool success{};
    LogIndex match_index{};
    LogIndex conflict_index{1};
};

using Message = std::variant<RequestVote, RequestVoteReply, AppendEntries, AppendEntriesReply>;

struct Envelope {
    NodeId from{};
    NodeId to{};
    Message message;
};

std::string role_name(Role role);
