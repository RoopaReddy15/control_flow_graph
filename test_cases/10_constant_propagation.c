// ============================================================
// Test Case 10: Constant Propagation (Phase 3)
//
// Verifies: Known constant values are substituted into
//   subsequent uses within the same block.
//   PDF spec: "If x = 10, replace all future uses of x with 10"
//
// Expected: After propagation + folding:
//   x=10, y=20, z=30, w=60 (all folded to constants)
//   The IF_COND should fold to a constant (10 > 5 → 1),
//   enabling branch pruning.
// ============================================================

int main() {
    int x = 10;
    int y = x + 10;       // propagate x=10 → y = 10+10 = 20
    int z = y + x;        // propagate x=10,y=20 → z = 20+10 = 30
    int w = x + y + z;    // → 10+20+30 = 60

    // Condition becomes constant after propagation
    if (x > 5) {
        w = w + 1;        // this branch always taken
    } else {
        w = 0;            // this branch NEVER taken → pruned
    }

    return w;
}
