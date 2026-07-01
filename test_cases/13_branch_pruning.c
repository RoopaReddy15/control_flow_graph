// ============================================================
// Test Case 13: Branch Pruning (Phase 3)
//
// Verifies: When a condition is folded to a constant, the dead
//   branch edge is removed and the condition is replaced with
//   [branch pruned]. Combined with unreachable removal, the
//   dead branch's blocks are deleted.
//
// Expected: debug_mode=0 → if(0) → TRUE branch pruned →
//           "dead branch" block removed entirely from CFG.
//           Always-true branch (if(1)) keeps only TRUE path.
// ============================================================

int main() {
    // Always-false branch
    int debug_mode = 0;
    if (debug_mode) {
        int dead_code = 999;  // entire block pruned + removed
    }

    // Always-true branch
    int always = 1;
    int result = 0;
    if (always) {
        result = 42;          // this path always taken
    } else {
        result = -1;          // this path pruned
    }

    return result;
}
