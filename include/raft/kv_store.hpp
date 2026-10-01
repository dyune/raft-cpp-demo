#pragma once

#include "raft/types.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace raft {
class KvStore {
public:
    void apply(const Command& command);
    [[nodiscard]] std::optional<std::string> get(const std::string& key) const;
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
private:
    std::unordered_map<std::string, std::string> data_;
};
} // namespace raft
