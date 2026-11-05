package com.proj.raftcli.raft.rpc;

public interface RPC {
    void requestVote(int to, RpcDTO.RequestVote req);
    boolean appendEntries(int to, RpcDTO.AppendEntries entries);
    void sendHeartbeat(int to, RpcDTO.Heartbeat heartbeat);
}
