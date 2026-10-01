#include "raft/cluster.hpp"
#include "raft/kv_store.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
template<class Predicate>
bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 3s) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (predicate()) return true;
        std::this_thread::sleep_for(10ms);
    }
    return false;
}
raft::Config fast_config(int nodes, std::uint64_t seed) {
    raft::Config c; c.nodes = nodes; c.seed = seed; c.min_delay = 1ms; c.max_delay = 4ms;
    c.election_min = 90ms; c.election_max = 170ms; c.heartbeat = 20ms; return c;
}
bool every_node_has(raft::Cluster& cluster, const std::string& key, const std::string& value) {
    const auto states = cluster.status();
    for (const auto& state : states) {
        if (!state.running || cluster.get_local(state.id, key) != value) return false;
    }
    return true;
}
void kv_unit_test() {
    raft::KvStore store; assert(!store.get("language"));
    store.apply({raft::Op::set, "language", "C++"}); assert(store.get("language") == "C++");
    store.apply({raft::Op::del, "language", {}}); assert(!store.get("language"));
}
void election_and_replication_test() {
    auto c = fast_config(3, 42);
    raft::Cluster cluster(c); cluster.start();
    assert(eventually([&] { return cluster.leader().has_value(); })); assert(cluster.set("x", "one"));
    assert(eventually([&] { return every_node_has(cluster, "x", "one"); }));
    assert(cluster.get("x") == "one");
}
void failover_and_repair_test() {
    auto c = fast_config(3, 7);
    raft::Cluster cluster(c); cluster.start(); assert(eventually([&] { return cluster.leader().has_value(); }));
    const int old = *cluster.leader(); cluster.crash(old);
    assert(eventually([&] { const auto id = cluster.leader(); return id && *id != old; }));
    assert(cluster.set("after", "failover")); cluster.recover(old);
    assert(eventually([&] { return every_node_has(cluster, "after", "failover"); }));
}
void partition_test() {
    auto c = fast_config(3, 99);
    raft::Cluster cluster(c); cluster.start(); assert(eventually([&] { return cluster.leader().has_value(); }));
    const int old = *cluster.leader(); cluster.isolate(old, true);
    assert(eventually([&] { for (auto s : cluster.status()) if (s.id != old && s.role == raft::Role::leader) return true; return false; }));
    assert(cluster.set("partition", "survived", 4s));
    cluster.isolate(old, false);
    assert(eventually([&] { int leaders = 0; for (auto s : cluster.status()) if (s.role == raft::Role::leader) ++leaders; return leaders == 1; }));
    assert(eventually([&] { return every_node_has(cluster, "partition", "survived"); }));
}
void packet_loss_test() {
    auto c = fast_config(3, 123); c.drop_rate = 0.25;
    raft::Cluster cluster(c); cluster.start(); assert(eventually([&] { return cluster.leader().has_value(); }, 5s));
    assert(cluster.set("lossy", "network", 5s));
}
void single_node_test() {
    auto c = fast_config(1, 5); raft::Cluster cluster(c); cluster.start();
    assert(eventually([&] { return cluster.leader().has_value(); }));
    assert(cluster.set("solo", "committed", 500ms));
    assert(cluster.get_local(0, "solo") == "committed");
}
void even_cluster_majority_test() {
    auto c = fast_config(4, 81); raft::Cluster cluster(c); cluster.start();
    assert(eventually([&] { return cluster.leader().has_value(); }));
    assert(cluster.set("four", "nodes"));
    assert(eventually([&] { return every_node_has(cluster, "four", "nodes"); }));

    for (int a : {0, 1}) for (int b : {2, 3}) cluster.partition(a, b, true);
    std::this_thread::sleep_for(400ms);
    assert(!cluster.set("minority", "must-not-commit", 600ms));
    for (int a : {0, 1}) for (int b : {2, 3}) cluster.partition(a, b, false);
}
void invalid_config_test() {
    auto c = fast_config(3, 1); c.drop_rate = 1.5;
    bool threw = false;
    try { raft::Cluster cluster(c); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    c = fast_config(3, 1); c.election_min = 10ms; c.heartbeat = 20ms; threw = false;
    try { raft::Cluster cluster(c); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}
void follower_commit_index_is_monotonic_test() {
    auto c = fast_config(2, 17);
    raft::Transport transport(c);
    raft::Node follower(0, {1}, transport, c);
    follower.start();

    raft::AppendEntries commit_one{
        1, 1, 0, 0,
        {{1, {raft::Op::set, "monotonic", "yes"}}},
        1
    };
    follower.receive({1, 0, commit_one});
    assert(eventually([&] { return follower.snapshot().commit_index == 1; }));

    // This delayed older heartbeat is still term 1 but knows a lower commit index.
    follower.receive({1, 0, raft::AppendEntries{1, 1, 0, 0, {}, 0}});
    std::this_thread::sleep_for(40ms);
    const auto snapshot = follower.snapshot();
    assert(snapshot.commit_index == 1);
    assert(snapshot.last_applied == 1);
    assert(follower.get("monotonic") == "yes");
}
int main() {
    kv_unit_test(); election_and_replication_test(); failover_and_repair_test(); partition_test();
    packet_loss_test(); single_node_test(); even_cluster_majority_test(); invalid_config_test();
    follower_commit_index_is_monotonic_test();
    std::cout << "all tests passed\n";
}
