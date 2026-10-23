#include "kv_store.h"

#include <stdexcept>

void KVStore::apply(const Command& command) {
    if (command.op == Op::set) {
        data_[command.key] = command.value;
    } else if (command.op == Op::del) {
        data_.erase(command.key);
    } else {
        throw std::invalid_argument("Invalid Op");
    }
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    const auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    return it->second;
}

bool KVStore::contains(const std::string& key) const noexcept {
    return data_.contains(key);
}
