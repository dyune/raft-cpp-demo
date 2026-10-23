package com.proj.raftcli.raft.entities.node;

import com.proj.raftcli.raft.entities.config.RunConfig;
import com.proj.raftcli.raft.entities.log.RaftLog;

import java.util.ArrayList;

public class Node {

    private int term;
    private NodeRole role;
    private ArrayList<Integer> peers;
    private RaftLog log;
    private RunConfig config;

    public Node(RunConfig config) {
        this.config = config;
    }

    public void start() {

    }

    public void crash() {

    }

    private void sendHeartbeat() {}



}
