// ============================================================
// Test Case 21: Empty Block Merging (Phase 3)
//
// Verifies: Blocks with no real statements (after optimization)
//   are merged by rewiring predecessors to their successor.
//
// Expected: After branch pruning removes the condition,
//   the empty condition block is merged away, reducing
//   total block count.
// ============================================================

int main() {
    int x = 1;

    // After propagation: if(1) → true branch pruned → condition
    // block becomes empty → merged away
    if (x) {
        x = x + 1;
    }

    // Another empty block scenario via nested if
    int y = 0;
    if (y) {
        // empty block (y=0, so this is pruned entirely)
    }

    return x;
}
