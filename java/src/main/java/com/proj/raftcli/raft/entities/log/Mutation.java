package com.proj.raftcli.raft.entities.log;

sealed interface Mutation {
    record Set(String key, String value) implements Mutation {}
    record Delete(String key) implements Mutation {}
    record NoOp() implements Mutation {}
}
