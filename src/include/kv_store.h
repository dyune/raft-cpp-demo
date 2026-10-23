#pragma once

#include "types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

class KVStore {
public:
    void apply(const Command& command);
    [[nodiscard]] std::optional<std::string> get(const std::string& key) const;
    [[nodiscard]] bool contains(const std::string& key) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept {
        return data_.size();
    }

private:
    std::unordered_map<std::string, std::string> data_;
};
