#pragma once

#include "raft/config.hpp"
#include "raft/types.hpp"
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <random>
#include <set>
#include <thread>
#include <utility>
#include <vector>

namespace raft {
class Transport {
public:
    using Receiver = std::function<void(Envelope)>;
    explicit Transport(const Config& config);
    ~Transport();
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    void set_receiver(Receiver receiver);
    void send(Envelope envelope);
    void partition(NodeId a, NodeId b, bool blocked);
    void set_drop_rate(double rate);
    void stop();
private:
    struct Pending {
        std::chrono::steady_clock::time_point due;
        std::uint64_t sequence{};
        Envelope envelope;
    };
    struct Later { bool operator()(const Pending& a, const Pending& b) const; };
    void run();
    bool blocked(NodeId a, NodeId b) const;
    const Config config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<Pending, std::vector<Pending>, Later> pending_;
    std::set<std::pair<NodeId, NodeId>> partitions_;
    Receiver receiver_;
    std::mt19937_64 random_;
    double drop_rate_;
    std::uint64_t sequence_{0};
    bool stopping_{false};
    std::thread thread_;
};
} // namespace raft
