# Raft C++ demo

Complete C++20 implementation of the course's in-process Raft key-value simulator.
Each node has its own worker, log, state machine, and inbox. A separate transport
thread simulates delay, packet loss, and bidirectional partitions.

## Build and test

Requires CMake 3.20+ and a C++20 compiler. From this project directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/raft_cpp_demo --nodes 3 --seed 42
```

In CLion, reload CMake and select the `raft_cpp_demo` run target. The `raft_tests`
target runs the integration suite. Existing IDE files are not changed.

## Try the simulator

```text
WAIT 700
STATUS
SET color blue
WAIT 200
GET color
DEL color
GET color
```

Expect a leader, successful writes, `blue`, then `(nil)`. Election timing and the
winning node vary with thread scheduling, even with a fixed random seed.

Use `STATUS` to find the leader ID. Run `CRASH id`, `WAIT 900`, `STATUS`, and a
new `SET` to observe failover. `RECOVER id` restarts that retained node; after a
short wait it should catch up. Use the actual numeric ID in place of `id`.

Commands: `SET key value`, `GET key`, `DEL key`, `STATUS`, `CRASH id`,
`RECOVER id`, `ISOLATE id`, `HEAL id`, `PARTITION a b`, `CONNECT a b`, `DROP p`,
`WAIT milliseconds`, `HELP`, `QUIT` (or `EXIT`). SET values may contain spaces.

Options: `--nodes`, `--seed`, `--drop`, `--delay-min`, `--delay-max`,
`--election-min`, `--election-max`, and `--heartbeat`. Time values are milliseconds.
Defaults: 3 nodes, seed 1, no loss, 5–25 ms delivery, 250–450 ms election timeout,
75 ms heartbeat. Configuration is validated before worker startup.

## Source map

- `src/include/types.h`: commands, log entries, vote and append RPCs.
- `src/kv_store.cpp`: deterministic SET/DEL application and local GET.
- `src/transport.cpp`: queued asynchronous delivery, delay, loss, partitions.
- `src/node.cpp`: lifecycle, elections, replication, majority commitment, application.
- `src/cluster.cpp`: ownership, routing, snapshots, fault controls.
- `src/main.cpp`: CLI and configuration parsing.
- `tests/tests.cpp`: KV, election, replication, failover/repair, partition, loss,
  single-node quorum, even-size quorum, invalid config, monotonic follower commit.

Headers retain the original `src/include/*.h` layout and global class names.
`Snapshot` now includes term, role, log size, commit index, and applied index.
`Node` now takes its ID, peer IDs, shared transport, and config in its constructor.

## Model and limits

Writes return OK only after commitment and local application. GET reads a locally
reported leader and is not linearizable during partitions. A timed-out write may
still take effect. There is no client deduplication or disk persistence.
CRASH stops a worker but retains its object; exiting the process loses all data.
Membership is fixed and IDs are dense from zero. Lifecycle operations are driven
serially by the CLI; the whole Cluster is not restartable after stop(), though
individual nodes can be recovered while it remains active.

## Sanitizers

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DRAFT_SANITIZER=address
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

For ThreadSanitizer use a separate `build-tsan` directory and
`-DRAFT_SANITIZER=thread`. Toolchain/platform support varies. Never combine ASan
and TSan. Assertions remain enabled in the teaching test target even in Release.
