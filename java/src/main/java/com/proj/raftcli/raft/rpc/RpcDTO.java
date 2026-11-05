package com.proj.raftcli.raft.rpc;

import com.proj.raftcli.raft.log.Mutation;

sealed public interface RpcDTO {
    // Heartbeat
    record Heartbeat(
            int term
    ) implements RpcDTO {}

    // Appends
    record AppendEntries(
            int term,
            int leaderId,
            int prevLogIdx,
            int prevLogTerm,
            int leaderCommitIdx,
            Mutation[] entries
    ) implements RpcDTO {}

    record AppendEntriesResult(
            int term,
            boolean success
    ) implements RpcDTO {}

    // Voting
    record RequestVote(
            int term,
            int candidateId,
            int lastLogTerm
    ) implements RpcDTO {}

    record VoteResult(
            int term,
            boolean voteGranted
    ) implements RpcDTO {}
}
