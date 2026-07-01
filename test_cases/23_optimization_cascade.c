// ============================================================
// Test Case 23: Iterative Optimization Cascade (Phase 3)
//
// Verifies: Multiple optimization passes interact and require
//   iteration to reach full simplification.
//
// Chain: propagation → folding → propagation → folding →
//        branch pruning → unreachable → DCE → merge
//
// Expected: After all iterations, the entire function reduces
//   to just ENTRY → "return 42" → EXIT (3 blocks, 2 edges).
// ============================================================

int main() {
    int a = 7;
    int b = a + 3;     // prop: b = 7+3, fold: b = 10
    int c = b * 2;     // prop: c = 10*2, fold: c = 20
    int d = c + a + b; // prop: d = 20+7+10, fold: d = 37
    int e = d + 5;     // prop: e = 37+5, fold: e = 42

    // Dead variables (DCE removes)
    int unused1 = a + b + c;
    int unused2 = unused1 * 2;

    // Constant branch (pruned)
    if (a > 100) {
        e = 0;         // never reached (a=7)
    }

    return e;
}
