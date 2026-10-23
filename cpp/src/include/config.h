#pragma once

#include <chrono>
#include <cstdint>

struct Config {
    int nodes{3};
    std::uint64_t seed{1};
    double drop_rate{0.0};
    std::chrono::milliseconds min_delay{5};
    std::chrono::milliseconds max_delay{25};
    std::chrono::milliseconds election_min{250};
    std::chrono::milliseconds election_max{450};
    std::chrono::milliseconds heartbeat{75};
};
