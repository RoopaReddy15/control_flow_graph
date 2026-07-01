// ============================================================
// Test Case 08: Goto and Labels (Phase 1)
//
// Verifies: goto creates edge from current block to label block.
//   Code between goto and label is unreachable.
//   Forward and backward gotos are resolved correctly.
//
// Expected: Edge from goto block → label block,
//           unreachable code between goto and label,
//           backward goto creates a loop-like structure.
// ============================================================

int main() {
    int x = 0;

    // Forward goto — skips unreachable code
    goto skip_ahead;
    x = 999;           // UNREACHABLE: should be removed
    x = 888;           // UNREACHABLE: should be removed

skip_ahead:
    x = 1;

    // Backward goto — creates a loop-like structure
    int count = 0;

loop_start:
    count = count + 1;
    if (count < 3) {
        goto loop_start;
    }

    return x + count;
}
