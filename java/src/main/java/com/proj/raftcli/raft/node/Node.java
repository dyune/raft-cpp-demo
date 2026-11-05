package com.proj.raftcli.raft.node;

import com.proj.raftcli.raft.entities.config.RunConfig;
import com.proj.raftcli.raft.log.RaftLog;
import com.proj.raftcli.raft.rpc.Envelope;
import com.proj.raftcli.raft.rpc.RpcDTO.*;
import com.proj.raftcli.raft.rpc.RPC;

import java.util.ArrayList;
import java.util.concurrent.BlockingDeque;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.TimeUnit;

public class Node {

    // Mutable states
    private int term;
    private NodeRole role;
    private ArrayList<Integer> peers;
    private RaftLog log;
    private long electionDeadlineMs;
    private long heartbeatDeadlineMs;

    // Election state
    private int votesReceived = 0;

    private final int id;
    private final RPC rpc;
    private final RunConfig config;
    private final BlockingDeque<Envelope> inbox = new LinkedBlockingDeque<>();

    public Node(
        int id,
        RunConfig config,
        RPC transport,
        long nowMs
    ) {
        this.id = id;
        this.config = config;
        this.rpc = transport;
        resetElectionTimer(nowMs);
    }

    /*
     * Begin operation of Raft lifecycle
     * */
    public void start() {
        try {
            while (!Thread.currentThread().isInterrupted()) {
                if (role == NodeRole.FOLLOWER) {
                    operateFollower();
                } else if (role == NodeRole.LEADER) {
                    operateLeader();
                } else {

                }
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    private void operateFollower() throws InterruptedException {
        long now = time();

        Envelope incoming = inbox.poll(10, TimeUnit.MILLISECONDS);

        if (incoming != null) {
            handleFollower(incoming);
        }

        if (electionTimeout(now)) {
            startElection();
        }



    }

    private void startElection() {
        role = NodeRole.CANDIDATE;
        term++;
        // Request votes
        votesReceived = 1;
        resetElectionTimer(time());
        for (int to : peers) {
            // Todo: lastLogTerm logic
            RequestVote req = new RequestVote(term, id, 999999);
            rpc.requestVote(to, req);
        }

    }

    private void handleFollower(Envelope env) {
        switch (env.message()) {
            case Heartbeat h -> {
                resetElectionTimer(time());
            }

            case AppendEntries ae -> {
                // TODO Append some entries
                resetElectionTimer(time());
            }

            case RequestVote rv -> {
                // TODO Implement vote logic

            }

            case VoteResult vr -> {

            }

            case AppendEntriesResult aer -> {

            }

            default -> throw new IllegalStateException("Unexpected value: " + env.message());
        }
    }

    private void operateLeader() {
        long now = time();
        if (now >= heartbeatDeadlineMs) {
            resetHeartbeatDeadline();
            for (int p : peers) {
                rpc.sendHeartbeat(p, new Heartbeat(term));
            }
        }
    }


    private void demoteToFollower(int newTerm) {
        role = NodeRole.FOLLOWER;
        resetElectionTimer(time());
    }

    private void promoteToLeader() {

    }

    private boolean electionTimeout(long time) { return time >= electionDeadlineMs && role == NodeRole.FOLLOWER; }

    private void resetElectionTimer(long nowMs) { electionDeadlineMs = nowMs + config.electionTimeout(); }

    private void resetHeartbeatDeadline() { heartbeatDeadlineMs = time() + config.heartBeatTimeout(); }

    private long time() { return System.nanoTime() / 1_000_000; }


}
