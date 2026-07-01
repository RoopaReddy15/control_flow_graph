// ============================================================
// Test Case 24: Compound Assignments (Phase 1)
//
// Verifies: +=, -=, *=, /=, %= are correctly converted to
//   x = x op rhs in the IR.
//
// Expected: Each compound assignment becomes a regular assignment
//   with the expanded expression (e.g., x += 5 → x = (x + 5)).
// ============================================================

int main() {
    int x = 10;
    x += 5;       // x = (x + 5) → 15
    x -= 3;       // x = (x - 3) → 12
    x *= 2;       // x = (x * 2) → 24
    x /= 4;       // x = (x / 4) → 6
    x %= 5;       // x = (x % 5) → 1
    return x;
}
