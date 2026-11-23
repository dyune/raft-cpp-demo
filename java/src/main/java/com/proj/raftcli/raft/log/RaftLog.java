package com.proj.raftcli.raft.log;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;

public class RaftLog {

    // Note that: 0th idx is a sentinel
    private final List<LogEntry> entries;

    public RaftLog() {
        this.entries = new ArrayList<>();
    }

    public boolean hasMatchingPrevEntry(int prevIdx, int prevTerm) {
        // TODO: Implement AE failure from leader side
        if (prevIdx == 0) {
            return prevTerm == 0;
        }

        if (prevIdx > entries.size()) {
            return false;
        }

        return entries.get(prevIdx - 1).term() == prevTerm;
    }

    public int firstConflictingOrEmptyEntry(List<LogEntry> ae, int prevLogIdx) {
        int startIdx = prevLogIdx + 1;

        for (int i = 0; i < ae.size(); i++) {
            int raftIdx = startIdx + i;
            int arrIdx = raftIdx - 1;

            // Our log is shorter than the leader's, automatically append
            if (arrIdx >= entries.size()) {
                return raftIdx;
            }

            LogEntry fromAe = ae.get(arrIdx);
            LogEntry fromLog = entries.get(arrIdx);

            // Conflict
            if (fromAe.term() != fromLog.term() || fromAe.mutation() != fromLog.mutation()) {
                return raftIdx;
            }
        }

        // No conflict
        return -1;
    }

    public Optional<LogEntry> entryAt(int idx) {
        // Not zero-indexed list, idx 0 represents empty, idx 1 represents first element.
        if (idx == 0) return Optional.empty();
        if (idx > entries.size() || idx < 0) throw new IllegalArgumentException("out of bounds");
        return Optional.of(entries.get(idx - 1));
    }

    // Last possible index, bc it is 1-indexed, just return size
    public int lastIndex() { return entries.size(); }

    // Find the term at a specific log index
    public int termAt(int idx) {
        if (idx == 0) return 0;
        if (idx > entries.size() || idx < 0) throw new IllegalArgumentException("out of bounds");
        return entries.get(idx - 1).term();
    }

    public int lastTerm() {
        return termAt(lastIndex());
    }

    public void append(LogEntry entry) {
        entries.add(Objects.requireNonNull(entry, "LogEntry can't be null"));
    }

    // Returns a copy of the entries from start (inclusive) to stop (exclusive),
    public List<LogEntry> spliceEntries(int start, int stop) {
        if (start < 1 || stop < start || stop - 1 > entries.size()) {
            throw new IllegalArgumentException(
                    "Invalid log range: start=" + start + ", stop=" + stop
                            + ", size=" + entries.size());
        }
        return new ArrayList<>(entries.subList(start - 1, stop - 1));
    }

    // Remove conflicting and uncommitted log entries
    public void truncateFrom(int idx) {
        if (idx < 1 || idx > entries.size() + 1) throw new IllegalArgumentException();
        entries.subList(idx - 1, entries.size()).clear();
    }

}
