package com.proj.raftcli.raft.rpc;

import com.proj.raftcli.raft.rpc.RpcDTO.*;

public interface RPC {
    void requestVote(int to, RequestVote req);
    void sendHeartbeat(int to, Heartbeat heartbeat);
    void sendVoteResult(int to, VoteResult result);
    boolean appendEntries(int to, AppendEntries entries);
}
