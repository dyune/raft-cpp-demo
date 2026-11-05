package com.proj.raftcli.raft.entities.config;

public record RunConfig(
        long heartBeatTimeout,
        long electionTimeout
) {}


