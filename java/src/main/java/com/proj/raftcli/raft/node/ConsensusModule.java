package com.proj.raftcli.raft.node;

import com.proj.raftcli.raft.entities.config.RunConfig;
import com.proj.raftcli.raft.log.LogEntry;
import com.proj.raftcli.raft.log.RaftLog;
import com.proj.raftcli.raft.rpc.RPC;
import com.proj.raftcli.raft.rpc.RpcDTO.*;

import java.util.ArrayList;
import java.util.List;

import static java.util.Objects.requireNonNull;

public class ConsensusModule {

    // Immutables
    private final int id;
    private final RPC rpc;

    // Mutable states
    private int term;
    private NodeRole role;
    private final ArrayList<Integer> peers;
    private final RaftLog log;
    private long electionDeadlineMs;
    private long heartbeatDeadlineMs;

    // Voting variables
    private int votesReceived = 0;
    private Integer votedFor = null;

    private final RunConfig config;

    public ConsensusModule(
        int id,
        List<Integer> peers,
        RunConfig config,
        RPC rpc
    ) {
        this.id = id;
        this.rpc = requireNonNull(rpc, "rpc");
        this.config = requireNonNull(config, "config");
        this.peers = new ArrayList<>(requireNonNull(peers, "peers"));
        this.log = new RaftLog();
        this.term = 0;
        this.role = NodeRole.FOLLOWER;
        resetElectionTimer(time());
    }

    private void handleVoteResult(VoteResult res) {
        if (res.term() > term) {
            becomeFollower(term);
            return;
        }

        if (res.voteGranted()) {
            votesReceived++;
            if (votesReceived >= (peers.size() / 2 + 1)) {
                becomeLeader();
            }
        }
    }

    private VoteResult handleVoteRequest(RequestVote req) {
        if (req.term() < term) {
            return new VoteResult(term, false);
        }

        if (req.term() > term) {
            becomeFollower(req.term());
            votedFor = null;
            votesReceived = 0;
        }

        boolean canVote = votedFor == null || votedFor == req.candidateId();

        boolean logUpToDate =
            req.lastLogTerm() > log.lastTerm() ||
                (req.lastLogTerm() == log.lastTerm() && req.lastLogIndex() >= log.lastIndex());

        boolean granted = canVote && logUpToDate;

        if (granted) {
            votedFor = req.candidateId();
            resetElectionTimer(time());
        }

        return new VoteResult(term, granted);
    }

    private AppendEntriesResult handleAppendEntriesRequest(AppendEntries req) {
        if (req.term() < term) {
            return new AppendEntriesResult(term, false);
        }

        else if (!log.hasMatchingPrevEntry(req.prevLogIdx(), req.prevLogTerm())) {
            return new AppendEntriesResult(term, false);
        }


        return null;
    }

    private void handleAppendEntriesResult(AppendEntriesResult res) {
        if (res.term() > term) {
            becomeFollower(res.term());
        }

        /*
        * handling AE retry logic to reach 0, 0 base case
        * prevLogIndex = nextIndex[followerId] - 1;
          prevLogTerm = prevLogIndex == 0 ? 0 : log.get(prevLogIndex - 1).term();
        *
        * */
    }

    private void startElection() {
        role = NodeRole.CANDIDATE;
        term++;
        votedFor = id;
        votesReceived = 1;

        resetElectionTimer(time());
        RequestVote req = new RequestVote(term, id, log.lastIndex(), log.lastTerm());

        for (int peer : peers) {
            rpc.requestVote(peer, req);
        }
    }

    private void appendEntries() {

    }

    private void sendHeartbeats() {
        resetHeartbeatDeadline();
        for (int peer: peers) {
            Heartbeat hb = new Heartbeat(term);
            rpc.sendHeartbeat(peer, hb);
        }
    }

    private void becomeFollower(int newTerm) {
        role = NodeRole.FOLLOWER;
        term = newTerm;
    }

    private void becomeLeader() {
        role = NodeRole.LEADER;
        sendHeartbeats();
    }

    /**
     * Checks time, determines if heartbeat or election must be done
     */
    public void tick() {
        if (role == NodeRole.LEADER) {
            if (heartbeatTimeout(time())) {
                sendHeartbeats();
            }
        } else if (role == NodeRole.CANDIDATE ) {
            if (electionTimeout(time())) {
                startElection();
            }
        }
    }

    private boolean electionTimeout(long time) { return time >= electionDeadlineMs && role == NodeRole.FOLLOWER; }

    private boolean heartbeatTimeout(long time) { return time >= heartbeatDeadlineMs && role == NodeRole.LEADER; }

    private void resetElectionTimer(long nowMs) { electionDeadlineMs = nowMs + config.electionTimeout(); }

    private void resetHeartbeatDeadline() { heartbeatDeadlineMs = time() + config.heartBeatTimeout(); }

    private long time() { return System.nanoTime() / 1_000_000; }


}
