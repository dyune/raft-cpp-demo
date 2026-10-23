package com.proj.raftcli.raft.entities.log;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class LogEntryTest {
    @Test
    void nullMutationIsRejected() {
        assertThrows(NullPointerException.class, () -> new LogEntry(1, null));
    }

    @Test
    void negativeTermIsRejected() {
        assertThrows(IllegalArgumentException.class,
                () -> new LogEntry(-1, new Mutation.NoOp()));
    }

    @Test
    void nullSetKeyIsRejected() {
        assertThrows(NullPointerException.class, () -> new Mutation.Set(null, "value"));
    }

    @Test
    void nullSetValueIsRejected() {
        assertThrows(NullPointerException.class, () -> new Mutation.Set("key", null));
    }

    @Test
    void nullDeleteKeyIsRejected() {
        assertThrows(NullPointerException.class, () -> new Mutation.Delete(null));
    }

    @Test
    void emptyValueIsDistinctFromMissingValue() {
        Mutation.Set mutation = new Mutation.Set("key", "");
        assertEquals("", mutation.value());
        assertEquals(mutation, new LogEntry(1, mutation).mutation());
    }

    @Test
    void noOpNeedsNoKeyOrValue() {
        LogEntry entry = new LogEntry(1, new Mutation.NoOp());
        assertEquals(new Mutation.NoOp(), entry.mutation());
    }
}
