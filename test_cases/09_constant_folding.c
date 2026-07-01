// ============================================================
// Test Case 09: Constant Folding (Phase 3)
//
// Verifies: All compile-time evaluable expressions are folded.
//   PDF spec: "Replacing x = 3 + 5 with x = 8"
//
// Expected optimized output:
//   a = 8, b = 6, c = 48, d = 4, e = 2
//   cmp1 = 1, cmp2 = 0, cmp3 = 1
//   logic = 0, neg = -5, bitnot = -11
// ============================================================

int main() {
    // Arithmetic
    int a = 3 + 5;         // → 8
    int b = 10 - 4;        // → 6
    int c = a * b;         // requires propagation first
    int d = 20 / 5;        // → 4
    int e = 17 % 3;        // → 2

    // Relational
    int cmp1 = (10 > 5);   // → 1
    int cmp2 = (3 == 4);   // → 0
    int cmp3 = (7 <= 7);   // → 1

    // Logical
    int logic = (1 && 0);  // → 0

    // Unary
    int neg = -(5);        // → -5
    int bitnot = ~(10);    // → -11

    return a + b + d + e + cmp1 + cmp2 + cmp3 + logic + neg + bitnot;
}
