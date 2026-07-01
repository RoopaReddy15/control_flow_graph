// ============================================================
// Test Case 04: For Loop (Phase 1 + Bonus: Loop Detection)
//
// Verifies: for(init; cond; incr) creates:
//   init → header(cond) → body → incr → header (back-edge) → exit
//
// Expected: Loop detected, increment block separate from body,
//           FOR_COND and FOR_NEXT instructions visible.
// ============================================================

int main() {
    int total = 0;

    for (int i = 0; i < 5; i++) {
        total = total + i;
    }

    return total;
}
