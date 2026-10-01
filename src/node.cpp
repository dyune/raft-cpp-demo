#include "raft/node.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <utility>

namespace {
std::mutex log_mutex;
}

namespace raft {
std::string role_name(Role role) {
    switch (role) {
        case Role::follower: return "follower";
        case Role::candidate: return "candidate";
        case Role::leader: return "leader";
    }
    return "unknown";
}

Node::Node(NodeId id, std::vector<NodeId> peers, Transport& transport,
           const Config& config)
    : id_(id), peers_(std::move(peers)), transport_(transport), config_(config),
      next_index_(static_cast<std::size_t>(config.nodes), 1),
      match_index_(static_cast<std::size_t>(config.nodes), 0),
      random_(config.seed + static_cast<std::uint64_t>(id) * 7919U) {}

Node::~Node() { crash(); }

void Node::start() {
    std::lock_guard lock(mutex_);
    if (running_) return;
    stop_requested_ = false; running_ = true; role_ = Role::follower;
    reset_election_deadline();
    thread_ = std::thread(&Node::run, this);
}

void Node::crash() {
    {
        std::lock_guard lock(mutex_);
        if (!running_) return;
        stop_requested_ = true; running_ = false; inbox_.clear();
    }
    cv_.notify_all(); commit_cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    log_event("crashed (volatile object retained)");
}

void Node::receive(Envelope envelope) {
    std::lock_guard lock(mutex_);
    if (!running_) return;
    inbox_.push_back(std::move(envelope)); cv_.notify_one();
}

NodeSnapshot Node::snapshot() const {
    std::lock_guard lock(mutex_);
    return {id_, current_term_, role_, running_, log_.size() - 1,
            commit_index_, last_applied_};
}

std::optional<std::string> Node::get(const std::string& key) const {
    std::lock_guard lock(mutex_); return store_.get(key);
}

bool Node::submit(Command command, std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    if (!running_ || role_ != Role::leader) return false;
    log_.push_back({current_term_, std::move(command)});
    const LogIndex wanted = log_.size() - 1;
    const Term submitted_term = current_term_;
    match_index_[static_cast<std::size_t>(id_)] = wanted;
    for (NodeId peer : peers_) send_append(peer);
    // A one-node cluster is already a majority; it receives no follower reply
    // that could otherwise trigger advance_commit().
    advance_commit();
    // The term check below ensures a replacement entry at the same index cannot
    // be mistaken for the command submitted by this caller.
    return commit_cv_.wait_for(lock, timeout, [&] {
        return !running_ || role_ != Role::leader || commit_index_ >= wanted;
    }) && commit_index_ >= wanted && term_at(wanted) == submitted_term;
}

void Node::reset_election_deadline() {
    std::uniform_int_distribution<long long> timeout(
        config_.election_min.count(), config_.election_max.count());
    election_deadline_ = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds(timeout(random_));
}

Term Node::term_at(LogIndex index) const { return index < log_.size() ? log_[index].term : 0; }

void Node::run() {
    std::unique_lock lock(mutex_);
    while (!stop_requested_) {
        const auto deadline = role_ == Role::leader ? heartbeat_deadline_ : election_deadline_;
        cv_.wait_until(lock, deadline, [&] { return stop_requested_ || !inbox_.empty(); });
        while (!inbox_.empty()) {
            Envelope envelope = std::move(inbox_.front()); inbox_.pop_front();
            std::visit([&](const auto& message) { handle(envelope.from, message); },
                       envelope.message);
        }
        const auto now = std::chrono::steady_clock::now();
        if (role_ == Role::leader && now >= heartbeat_deadline_) send_heartbeats();
        else if (role_ != Role::leader && now >= election_deadline_) begin_election();
    }
}

void Node::begin_election() {
    role_ = Role::candidate; ++current_term_; voted_for_ = id_;
    votes_received_.clear(); votes_received_.insert(id_);
    reset_election_deadline(); log_event("starts election");
    const RequestVote vote{current_term_, id_, log_.size() - 1, term_at(log_.size() - 1)};
    for (NodeId peer : peers_) transport_.send({id_, peer, vote});
    if (has_majority(votes_received_.size())) become_leader();
}

void Node::become_follower(Term term) {
    if (term > current_term_) { current_term_ = term; voted_for_.reset(); }
    role_ = Role::follower; votes_received_.clear(); reset_election_deadline();
    commit_cv_.notify_all();
}

void Node::become_leader() {
    role_ = Role::leader;
    const LogIndex next = log_.size();
    std::fill(next_index_.begin(), next_index_.end(), next);
    std::fill(match_index_.begin(), match_index_.end(), 0);
    match_index_[static_cast<std::size_t>(id_)] = next - 1;
    log_event("becomes leader"); send_heartbeats();
}

void Node::send_heartbeats() {
    for (NodeId peer : peers_) send_append(peer);
    heartbeat_deadline_ = std::chrono::steady_clock::now() + config_.heartbeat;
}

void Node::send_append(NodeId peer) {
    const auto p = static_cast<std::size_t>(peer);
    const LogIndex next = next_index_[p];
    std::vector<LogEntry> entries(log_.begin() + static_cast<std::ptrdiff_t>(next), log_.end());
    transport_.send({id_, peer, AppendEntries{current_term_, id_, next - 1,
                    term_at(next - 1), std::move(entries), commit_index_}});
}

void Node::handle(NodeId from, const RequestVote& message) {
    if (message.term < current_term_) {
        transport_.send({id_, from, RequestVoteReply{current_term_, false}}); return;
    }
    if (message.term > current_term_) become_follower(message.term);
    const LogIndex mine = log_.size() - 1;
    const bool up_to_date = message.last_log_term > term_at(mine) ||
        (message.last_log_term == term_at(mine) && message.last_log_index >= mine);
    const bool grant = up_to_date && (!voted_for_ || *voted_for_ == message.candidate);
    if (grant) { voted_for_ = message.candidate; reset_election_deadline(); }
    transport_.send({id_, from, RequestVoteReply{current_term_, grant}});
}

void Node::handle(NodeId from, const RequestVoteReply& message) {
    if (message.term > current_term_) { become_follower(message.term); return; }
    if (role_ != Role::candidate || message.term != current_term_ || !message.granted) return;
    votes_received_.insert(from);
    if (has_majority(votes_received_.size())) become_leader();
}

void Node::handle(NodeId from, const AppendEntries& message) {
    if (message.term < current_term_) {
        transport_.send({id_, from, AppendEntriesReply{current_term_, false, 0, log_.size()}}); return;
    }
    if (message.term > current_term_ || role_ != Role::follower) become_follower(message.term);
    reset_election_deadline();
    if (message.prev_log_index >= log_.size() ||
        term_at(message.prev_log_index) != message.prev_log_term) {
        LogIndex conflict = std::min(message.prev_log_index, log_.size() - 1);
        if (message.prev_log_index < log_.size()) {
            const Term bad = term_at(message.prev_log_index);
            while (conflict > 1 && term_at(conflict - 1) == bad) --conflict;
        } else conflict = log_.size();
        transport_.send({id_, from, AppendEntriesReply{current_term_, false, 0, conflict}}); return;
    }
    LogIndex index = message.prev_log_index + 1;
    std::size_t incoming = 0;
    while (incoming < message.entries.size() && index < log_.size()) {
        if (log_[index].term != message.entries[incoming].term) {
            log_.erase(log_.begin() + static_cast<std::ptrdiff_t>(index), log_.end()); break;
        }
        ++index; ++incoming;
    }
    log_.insert(log_.end(), message.entries.begin() + static_cast<std::ptrdiff_t>(incoming),
                message.entries.end());
    // commitIndex is monotonic even when same-term heartbeats arrive out of order.
    if (message.leader_commit > commit_index_) {
        commit_index_ = std::min(message.leader_commit, log_.size() - 1);
    }
    apply_committed();
    transport_.send({id_, from, AppendEntriesReply{current_term_, true,
                     message.prev_log_index + message.entries.size(), 1}});
}

void Node::handle(NodeId from, const AppendEntriesReply& message) {
    if (message.term > current_term_) { become_follower(message.term); return; }
    if (role_ != Role::leader || message.term != current_term_) return;
    const auto p = static_cast<std::size_t>(from);
    if (message.success) {
        match_index_[p] = std::max(match_index_[p], message.match_index);
        next_index_[p] = match_index_[p] + 1; advance_commit();
    // Ignore a delayed failure that predates a success already recorded here.
    } else if (message.conflict_index > match_index_[p] &&
               message.conflict_index < next_index_[p]) {
        next_index_[p] = std::max<LogIndex>(1, std::min(next_index_[p] - 1, message.conflict_index));
        send_append(from);
    }
}

void Node::advance_commit() {
    for (LogIndex candidate = log_.size() - 1; candidate > commit_index_; --candidate) {
        if (term_at(candidate) != current_term_) continue;
        std::size_t count = 1;
        for (NodeId peer : peers_) if (match_index_[static_cast<std::size_t>(peer)] >= candidate) ++count;
        if (has_majority(count)) { commit_index_ = candidate; apply_committed(); commit_cv_.notify_all(); break; }
    }
}

void Node::apply_committed() {
    assert(last_applied_ <= commit_index_);
    assert(commit_index_ < log_.size());
    while (last_applied_ < commit_index_) store_.apply(log_[++last_applied_].command);
}

bool Node::has_majority(std::size_t count) const noexcept {
    // peers_ excludes this node, so quorum must use total membership.
    const std::size_t cluster_size = peers_.size() + 1;
    return count >= cluster_size / 2 + 1;
}

void Node::log_event(const std::string& event) const {
    std::lock_guard lock(log_mutex);
    std::clog << "node=" << id_ << " term=" << current_term_
              << " role=" << role_name(role_)
              << " event=\"" << event << "\"\n";
}
} // namespace raft
