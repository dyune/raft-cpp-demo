package com.proj.raftcli.raft.entities.log;

public record LogEntry(
        int term,
        Mutation mutation
) {}
