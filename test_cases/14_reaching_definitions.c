// ============================================================
// Test Case 14: Reaching Definitions Analysis (Phase 2)
//
// Verifies: For each variable at each point, which definitions
//   can reach it. Multiple paths create multiple reaching defs.
//
// Expected:
//   Block after if/else merge: x has TWO reaching definitions
//   (one from then-branch, one from else-branch).
//   After the merge, y's definition only comes from one place.
// ============================================================

int main() {
    int x = 0;     // def1 of x

    if (x == 0) {
        x = 10;    // def2 of x (then-branch)
    } else {
        x = 20;    // def3 of x (else-branch)
    }
    // At this point: reaching defs of x = {def2, def3}

    int y = x + 1; // uses x — both defs reach here

    return y;
}
