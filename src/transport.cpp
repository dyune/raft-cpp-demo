#include "transport.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

bool Transport::Later::operator()(const Pending& a, const Pending& b) const {
    if (a.due != b.due) {
        return a.due > b.due;
    }

    return a.sequence > b.sequence;
}

Transport::Transport(const Config& config)
    : config_(config), random_(config.seed ^ 0x9e3779b97f4a7c15ULL), drop_rate_(config.drop_rate) {
    if (!std::isfinite(drop_rate_) || drop_rate_ < 0.0 || drop_rate_ > 1.0) {
        throw std::invalid_argument("drop rate must be between 0 and 1");
    }

    if (config_.min_delay.count() < 0 || config_.max_delay < config_.min_delay) {
        throw std::invalid_argument("delay range must satisfy 0 <= min <= max");
    }

    thread_ = std::thread(&Transport::run, this);
}

Transport::~Transport() {
    stop();
}

void Transport::set_receiver(Receiver receiver) {
    std::lock_guard lock(mutex_);

    receiver_ = std::move(receiver);
}

bool Transport::blocked(NodeId a, NodeId b) const {
    return partitions_.contains(std::minmax(a, b));
}

void Transport::send(Envelope envelope) {
    std::lock_guard lock(mutex_);

    std::bernoulli_distribution drop(drop_rate_);

    if (stopping_ || blocked(envelope.from, envelope.to) || drop(random_)) {
        return;
    }

    const auto lo = config_.min_delay.count();
    const auto hi = config_.max_delay.count();
    std::uniform_int_distribution<long long> delay(lo, hi);
    pending_.push({std::chrono::steady_clock::now() + std::chrono::milliseconds(delay(random_)),
                   sequence_++,
                   std::move(envelope)});
    cv_.notify_one();
}

void Transport::partition(NodeId a, NodeId b, bool value) {
    std::lock_guard lock(mutex_);

    const auto edge = std::minmax(a, b);

    if (value) {
        partitions_.insert(edge);
    } else {
        partitions_.erase(edge);
    }
}

void Transport::set_drop_rate(double rate) {
    if (!std::isfinite(rate) || rate < 0.0 || rate > 1.0) {
        throw std::invalid_argument("drop rate must be between 0 and 1");
    }

    std::lock_guard lock(mutex_);

    drop_rate_ = rate;
}

void Transport::run() {
    std::unique_lock lock(mutex_);

    while (!stopping_) {
        if (pending_.empty()) {
            cv_.wait(lock);
            continue;
        }

        const auto due = pending_.top().due;

        if (cv_.wait_until(lock, due) != std::cv_status::timeout) {
            continue;
        }

        auto item = pending_.top();
        pending_.pop();
        auto receiver = receiver_;

        if (blocked(item.envelope.from, item.envelope.to)) {
            continue;
        }

        lock.unlock();

        if (receiver) {
            receiver(std::move(item.envelope));
        }

        lock.lock();
    }
}

void Transport::stop() {
    {
        std::lock_guard lock(mutex_);

        if (stopping_) {
            return;
        }

        stopping_ = true;
    }

    cv_.notify_all();

    if (thread_.joinable()) {
        thread_.join();
    }
}
