package com.proj.raftcli.raft.rpc;

public record Envelope(
        int from,
        RpcDTO message
) {}
