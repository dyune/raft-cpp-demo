#include "raft/kv_store.hpp"

namespace raft {
void KvStore::apply(const Command& command) {
    if (command.op == Op::set) data_[command.key] = command.value;
    else data_.erase(command.key);
}

std::optional<std::string> KvStore::get(const std::string& key) const {
    const auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    return it->second;
}
} // namespace raft

