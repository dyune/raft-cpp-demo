package com.proj.raftcli.raft.entities.log;

import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

/** Behavioral contracts for the in-memory log; no Spring context is required. */
class RaftLogTest {
    private final LogEntry first = new LogEntry(1, new Mutation.Set("x", "10"));
    private final LogEntry second = new LogEntry(1, new Mutation.Set("y", "20"));
    private final LogEntry third = new LogEntry(2, new Mutation.Delete("x"));

    private RaftLog populatedLog() {
        RaftLog log = new RaftLog();
        log.append(first);
        log.append(second);
        log.append(third);
        return log;
    }

    @Test
    void emptyLogHasOnlyAVirtualSentinel() {
        RaftLog log = new RaftLog();
        assertEquals(0, log.lastIndex());
        assertEquals(0, log.termAt(0));
        assertTrue(log.entryAt(0).isEmpty());
        assertEquals(List.of(), log.spliceEntries(1, 1));
        assertThrows(IllegalArgumentException.class, () -> log.entryAt(1));
        assertThrows(IllegalArgumentException.class, () -> log.termAt(1));
    }

    @Test
    void appendAssignsConsecutiveOneBasedPositions() {
        RaftLog log = populatedLog();
        assertEquals(3, log.lastIndex());
        assertEquals(first, log.entryAt(1).orElseThrow());
        assertEquals(second, log.entryAt(2).orElseThrow());
        assertEquals(third, log.entryAt(3).orElseThrow());
        assertEquals(1, log.termAt(1));
        assertEquals(1, log.termAt(2));
        assertEquals(2, log.termAt(3));
        assertTrue(log.entryAt(0).isEmpty());
        assertEquals(0, log.termAt(0));
    }

    @Test
    void invalidEntryIndicesFailExplicitly() {
        RaftLog log = populatedLog();
        for (int index : new int[]{-1, Integer.MIN_VALUE, 4, Integer.MAX_VALUE}) {
            assertThrows(IllegalArgumentException.class, () -> log.entryAt(index),
                    "index=" + index);
        }
    }

    @Test
    void invalidTermIndicesFailExplicitly() {
        RaftLog log = populatedLog();
        for (int index : new int[]{-1, Integer.MIN_VALUE, 4, Integer.MAX_VALUE}) {
            assertThrows(IllegalArgumentException.class, () -> log.termAt(index),
                    "index=" + index);
        }
    }

    @Test
    void rangeUsesInclusiveStartAndExclusiveStop() {
        RaftLog log = populatedLog();
        assertEquals(List.of(first, second, third), log.spliceEntries(1, 4));
        assertEquals(List.of(second), log.spliceEntries(2, 3));
        assertEquals(List.of(third), log.spliceEntries(3, 4));
        assertTrue(log.spliceEntries(2, 2).isEmpty());
        assertTrue(log.spliceEntries(4, 4).isEmpty());
    }

    @Test
    void invalidRangesAreRejectedWithoutChangingTheLog() {
        RaftLog log = populatedLog();
        for (int[] range : new int[][]{{0, 1}, {-1, 2}, {3, 2}, {1, 5}, {5, 5}}) {
            assertThrows(IllegalArgumentException.class,
                    () -> log.spliceEntries(range[0], range[1]));
        }
        assertEquals(List.of(first, second, third), log.spliceEntries(1, 4));
    }

    @Test
    void returnedBatchCannotMutateTheLog() {
        RaftLog log = populatedLog();
        List<LogEntry> batch = log.spliceEntries(1, 4);
        // Both a mutable defensive copy and an immutable copy satisfy isolation.
        try {
            batch.clear();
        } catch (UnsupportedOperationException expectedForImmutableCopy) {
            // Immutable snapshots are also valid.
        }
        assertEquals(List.of(first, second, third), log.spliceEntries(1, 4));
    }

    @Test
    void returnedBatchRemainsStableAfterFurtherAppends() {
        RaftLog log = populatedLog();
        List<LogEntry> batch = log.spliceEntries(1, 4);
        log.append(new LogEntry(3, new Mutation.NoOp()));
        assertEquals(List.of(first, second, third), batch);
        assertEquals(4, log.lastIndex());
    }

    @Test
    void truncateMiddleRemovesTheEntireSuffixAndPreservesThePrefix() {
        RaftLog log = populatedLog();
        log.append(new LogEntry(2, new Mutation.Set("z", "30")));
        log.append(new LogEntry(2, new Mutation.NoOp()));
        List<LogEntry> oldBatch = log.spliceEntries(1, 6);
        log.truncateFrom(3);
        assertEquals(2, log.lastIndex());
        assertEquals(List.of(first, second), log.spliceEntries(1, 3));
        assertEquals(5, oldBatch.size());
        assertEquals(third, oldBatch.get(2));
    }

    @Test
    void truncateFirstEmptiesTheLog() {
        RaftLog log = populatedLog();
        log.truncateFrom(1);
        assertEquals(0, log.lastIndex());
        assertEquals(0, log.termAt(log.lastIndex()));
        assertTrue(log.spliceEntries(1, 1).isEmpty());
    }

    @Test
    void truncateLastRemovesExactlyOneEntry() {
        RaftLog log = populatedLog();
        log.truncateFrom(3);
        assertEquals(2, log.lastIndex());
        assertEquals(second, log.entryAt(2).orElseThrow());
        assertEquals(1, log.termAt(log.lastIndex()));
    }

    @Test
    void truncateImmediatelyAfterLastIsANoOp() {
        RaftLog log = populatedLog();
        log.truncateFrom(4);
        assertEquals(List.of(first, second, third), log.spliceEntries(1, 4));
    }

    @Test
    void truncateEmptySuffixOfEmptyLogIsANoOp() {
        RaftLog log = new RaftLog();
        log.truncateFrom(1);
        assertEquals(0, log.lastIndex());
    }

    @Test
    void invalidTruncationDoesNotChangeEntries() {
        RaftLog log = populatedLog();
        for (int index : new int[]{-1, 0, 5, Integer.MAX_VALUE}) {
            assertThrows(IllegalArgumentException.class, () -> log.truncateFrom(index));
        }
        assertEquals(List.of(first, second, third), log.spliceEntries(1, 4));
    }

    @Test
    void replacementReusesTheFirstRemovedIndex() {
        RaftLog log = populatedLog();
        LogEntry replacement = new LogEntry(3, new Mutation.Set("x", "40"));
        log.truncateFrom(3);
        log.append(replacement);
        assertEquals(3, log.lastIndex());
        assertEquals(List.of(first, second, replacement), log.spliceEntries(1, 4));
        assertEquals(3, log.termAt(3));
    }

    @Test
    void nullAppendIsRejectedWithoutChangingTheLog() {
        RaftLog log = populatedLog();
        assertThrows(NullPointerException.class, () -> log.append(null));
        assertEquals(3, log.lastIndex());
    }
}
