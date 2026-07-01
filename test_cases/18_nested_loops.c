// ============================================================
// Test Case 18: Loop Detection — Dominator Trees (Bonus)
//
// Verifies: Dominator tree computation and back-edge detection
//   for nested loops. PDF spec: "Use Dominator Trees to find
//   loops in the CFG."
//
// Expected:
//   - 2 loops detected (outer while, inner for)
//   - Correct dominator sets (outer header dominates inner)
//   - Back-edges identified for both loops
// ============================================================

int main() {
    int i = 0;
    int total = 0;

    // Outer loop
    while (i < 3) {
        // Inner loop
        for (int j = 0; j < 5; j++) {
            total = total + 1;
        }
        i = i + 1;
    }

    return total;
}
