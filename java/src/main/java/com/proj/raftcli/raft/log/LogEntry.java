package com.proj.raftcli.raft.log;

public record LogEntry(
        int term,
        Mutation mutation
) {}
