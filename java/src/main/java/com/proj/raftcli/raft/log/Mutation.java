package com.proj.raftcli.raft.log;

sealed public interface Mutation {
    record Set(String key, String value) implements Mutation {}
    record Delete(String key) implements Mutation {}
    record NoOp() implements Mutation {}
}
