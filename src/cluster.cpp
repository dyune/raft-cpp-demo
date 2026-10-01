#include "raft/cluster.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>
namespace {
raft::Config validate_config(raft::Config config) {
    // Validate before Transport starts a worker or constructs distributions.
    if (config.nodes < 1) throw std::invalid_argument("nodes must be positive");
    if (!std::isfinite(config.drop_rate) || config.drop_rate < 0.0 || config.drop_rate > 1.0)
        throw std::invalid_argument("drop rate must be between 0 and 1");
    if (config.min_delay.count() < 0 || config.max_delay < config.min_delay)
        throw std::invalid_argument("delay range must satisfy 0 <= min <= max");
    if (config.heartbeat.count() <= 0 || config.election_min <= config.heartbeat ||
        config.election_max < config.election_min)
        throw std::invalid_argument("timings must satisfy 0 < heartbeat < election-min <= election-max");
    return config;
}
}
namespace raft {
Cluster::Cluster(Config config) : config_(validate_config(std::move(config))), transport_(config_) {
    for (NodeId id = 0; id < config_.nodes; ++id) {
        std::vector<NodeId> peers;
        for (NodeId peer = 0; peer < config_.nodes; ++peer) if (peer != id) peers.push_back(peer);
        nodes_.push_back(std::make_unique<Node>(id, std::move(peers), transport_, config_));
    }
    transport_.set_receiver([this](Envelope e) { if (Node* n = node(e.to)) n->receive(std::move(e)); });
}
Cluster::~Cluster() { stop(); }
Node* Cluster::node(NodeId id) const {
    if (id < 0 || id >= static_cast<NodeId>(nodes_.size())) return nullptr;
    return nodes_[static_cast<std::size_t>(id)].get();
}
void Cluster::start() { for (auto& n : nodes_) n->start(); }
void Cluster::stop() { for (auto& n : nodes_) n->crash(); transport_.stop(); }
std::optional<NodeId> Cluster::leader() const {
    for (const auto& n : nodes_) { const auto s = n->snapshot(); if (s.running && s.role == Role::leader) return s.id; }
    return std::nullopt;
}
bool Cluster::set(std::string k, std::string v, std::chrono::milliseconds t) {
    const auto states = status();
    std::vector<NodeId> candidates;
    for (const auto& s : states) if (s.running && s.role == Role::leader) candidates.push_back(s.id);
    if (candidates.empty()) return false;
    const auto slice = t / static_cast<int>(candidates.size());
    for (NodeId id : candidates)
        if (node(id)->submit({Op::set, k, v}, slice)) return true;
    return false;
}
bool Cluster::del(std::string k, std::chrono::milliseconds t) {
    const auto states = status();
    std::vector<NodeId> candidates;
    for (const auto& s : states) if (s.running && s.role == Role::leader) candidates.push_back(s.id);
    if (candidates.empty()) return false;
    const auto slice = t / static_cast<int>(candidates.size());
    for (NodeId id : candidates)
        if (node(id)->submit({Op::del, k, {}}, slice)) return true;
    return false;
}
std::optional<std::string> Cluster::get(const std::string& k) const {
    const auto id = leader(); return id ? node(*id)->get(k) : std::nullopt;
}
std::optional<std::string> Cluster::get_local(NodeId id, const std::string& k) const {
    const Node* n = node(id); return n ? n->get(k) : std::nullopt;
}
void Cluster::crash(NodeId id) { if (Node* n = node(id)) n->crash(); }
void Cluster::recover(NodeId id) { if (Node* n = node(id)) n->start(); }
void Cluster::partition(NodeId a, NodeId b, bool value) { transport_.partition(a, b, value); }
void Cluster::isolate(NodeId id, bool value) { for (NodeId p = 0; p < config_.nodes; ++p) if (p != id) partition(id, p, value); }
void Cluster::set_drop_rate(double rate) { transport_.set_drop_rate(rate); }
std::vector<NodeSnapshot> Cluster::status() const {
    std::vector<NodeSnapshot> out; for (const auto& n : nodes_) out.push_back(n->snapshot()); return out;
}
} // namespace raft
