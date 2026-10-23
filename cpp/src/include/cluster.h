#pragma once

#include "node.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Cluster {
public:
    explicit Cluster(Config config);
    ~Cluster();

    Cluster(const Cluster&) = delete;
    Cluster& operator=(const Cluster&) = delete;

    void start();
    void stop();

    [[nodiscard]] std::optional<NodeId> leader() const;
    bool set(std::string key,
             std::string value,
             std::chrono::milliseconds timeout = std::chrono::seconds(2));
    bool del(std::string key, std::chrono::milliseconds timeout = std::chrono::seconds(2));
    [[nodiscard]] std::optional<std::string> get(const std::string& key) const;
    [[nodiscard]] std::optional<std::string> get_local(NodeId id, const std::string& key) const;
    void crash(NodeId id);
    void recover(NodeId id);
    void isolate(NodeId id, bool isolated);
    void partition(NodeId a, NodeId b, bool blocked);
    void set_drop_rate(double rate);
    [[nodiscard]] std::vector<Snapshot> status() const;

private:
    Node* node(NodeId id) const;

    Config config_;
    Transport transport_;
    std::vector<std::unique_ptr<Node>> nodes_;
};
