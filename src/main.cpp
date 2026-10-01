#include "raft/cluster.hpp"
#include <cmath>
#include <chrono>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
namespace {
void help() {
    std::cout << "Commands:\n  SET key value\n  GET key\n  DEL key\n  STATUS\n"
              << "  CRASH id | RECOVER id | ISOLATE id | HEAL id\n"
              << "  PARTITION a b | CONNECT a b | DROP probability\n"
              << "  WAIT milliseconds | HELP | QUIT\n";
}
}
int main(int argc, char** argv) {
    try {
    raft::Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc) config.seed = std::stoull(argv[++i]);
        else if (arg == "--nodes" && i + 1 < argc) config.nodes = std::stoi(argv[++i]);
        else if (arg == "--drop" && i + 1 < argc) config.drop_rate = std::stod(argv[++i]);
        else if (arg == "--delay-min" && i + 1 < argc) config.min_delay = std::chrono::milliseconds(std::stoll(argv[++i]));
        else if (arg == "--delay-max" && i + 1 < argc) config.max_delay = std::chrono::milliseconds(std::stoll(argv[++i]));
        else if (arg == "--election-min" && i + 1 < argc) config.election_min = std::chrono::milliseconds(std::stoll(argv[++i]));
        else if (arg == "--election-max" && i + 1 < argc) config.election_max = std::chrono::milliseconds(std::stoll(argv[++i]));
        else if (arg == "--heartbeat" && i + 1 < argc) config.heartbeat = std::chrono::milliseconds(std::stoll(argv[++i]));
        else { std::cerr << "usage: raft_kv [--seed N] [--nodes N] [--drop P] [--delay-min MS] [--delay-max MS] [--election-min MS] [--election-max MS] [--heartbeat MS]\n"; return 2; }
    }
    raft::Cluster cluster(config); cluster.start();
    std::cout << "Raft KV simulation (seed=" << config.seed << "). Type HELP.\n";
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream in(line); std::string cmd; in >> cmd;
        if (cmd == "SET") {
            std::string key, value; in >> key; std::getline(in >> std::ws, value);
            std::cout << ((!key.empty() && !value.empty() && cluster.set(key, value)) ? "OK\n" : "NO LEADER/TIMEOUT\n");
        } else if (cmd == "GET") {
            std::string key; in >> key;
            if (key.empty()) { std::cout << "usage: GET key\n"; continue; }
            if (!cluster.leader()) { std::cout << "NO LEADER\n"; continue; }
            const auto v = cluster.get(key); std::cout << (v ? *v : "(nil)") << '\n';
        } else if (cmd == "DEL") {
            std::string key; in >> key;
            if (key.empty()) { std::cout << "usage: DEL key\n"; continue; }
            std::cout << (cluster.del(key) ? "OK\n" : "NO LEADER/TIMEOUT\n");
        } else if (cmd == "STATUS") {
            for (const auto& s : cluster.status()) std::cout << "node=" << s.id << " running=" << std::boolalpha << s.running
                << " term=" << s.term << " role=" << raft::role_name(s.role) << " log=" << s.log_size
                << " commit=" << s.commit_index << " applied=" << s.last_applied << '\n';
        } else if (cmd == "CRASH" || cmd == "RECOVER" || cmd == "ISOLATE" || cmd == "HEAL") {
            int id; if (!(in >> id) || id < 0 || id >= config.nodes) { std::cout << "usage: " << cmd << " id (0.." << config.nodes - 1 << ")\n"; continue; }
            if (cmd == "CRASH") cluster.crash(id); else if (cmd == "RECOVER") cluster.recover(id);
            else cluster.isolate(id, cmd == "ISOLATE"); std::cout << "OK\n";
        } else if (cmd == "PARTITION" || cmd == "CONNECT") {
            int a, b; if (!(in >> a >> b) || a < 0 || b < 0 || a >= config.nodes || b >= config.nodes || a == b) { std::cout << "usage: " << cmd << " distinct-node-a distinct-node-b\n"; continue; }
            cluster.partition(a, b, cmd == "PARTITION"); std::cout << "OK\n";
        } else if (cmd == "DROP") {
            double rate; if (!(in >> rate)) { std::cout << "usage: DROP probability\n"; continue; }
            if (!std::isfinite(rate) || rate < 0.0 || rate > 1.0) {
                std::cout << "usage: DROP probability-in-[0,1]\n"; continue;
            }
            cluster.set_drop_rate(rate); std::cout << "OK\n";
        } else if (cmd == "WAIT") {
            long long ms; if (!(in >> ms) || ms < 0) { std::cout << "usage: WAIT nonnegative-milliseconds\n"; continue; }
            std::this_thread::sleep_for(std::chrono::milliseconds(ms)); std::cout << "OK\n";
        } else if (cmd == "HELP") help();
        else if (cmd == "QUIT" || cmd == "EXIT") break;
        else if (!cmd.empty()) std::cout << "unknown command; type HELP\n";
    }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
