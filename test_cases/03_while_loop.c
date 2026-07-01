// ============================================================
// Test Case 03: While Loop (Phase 1 + Bonus: Loop Detection)
//
// Verifies: while creates header → body → back-edge → exit.
//   The back-edge (body → header) should be detected by the
//   dominator-based loop detector.
//
// Expected: Loop detected with header block, back-edge visible,
//           dominator sets show header dominates body.
// ============================================================

int main() {
    int i = 0;
    int sum = 0;

    while (i < 10) {
        sum = sum + i;
        i = i + 1;
    }

    return sum;
}
