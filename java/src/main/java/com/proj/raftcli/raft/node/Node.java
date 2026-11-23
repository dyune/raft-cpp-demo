package com.proj.raftcli.raft.node;

import com.proj.raftcli.raft.entities.config.RunConfig;
import com.proj.raftcli.raft.log.RaftLog;
import com.proj.raftcli.raft.rpc.Envelope;
import com.proj.raftcli.raft.rpc.RpcDTO.*;
import com.proj.raftcli.raft.rpc.RPC;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingDeque;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.TimeUnit;

public class Node {

    private final BlockingDeque<Envelope> inbox = new LinkedBlockingDeque<>();
    private final int id;
    private final ConsensusModule cm;

    public Node(
        int id,
        RunConfig config,
        RPC rpc,
        List<Integer> peers
    ) {
        this.id = id;
        this.cm = new ConsensusModule(
            id,
            peers,
            config,
            rpc
        );
    }

    /*
     * Begin operation of Raft lifecycle
     *
     * */
    public void start() {
        try {
            while (!Thread.currentThread().isInterrupted()) {
                Envelope env = inbox.poll(10, TimeUnit.MILLISECONDS);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    public void crash() {

    }
}
