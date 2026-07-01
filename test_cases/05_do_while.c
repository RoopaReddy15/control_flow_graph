// ============================================================
// Test Case 05: Do-While Loop (Phase 1)
//
// Verifies: do-while executes body FIRST, then checks condition.
//   body → cond → (body [back-edge] | exit)
//
// Expected: Body block appears before condition block,
//           DOWHILE_COND instruction in condition block.
// ============================================================

int main() {
    int n = 5;

    do {
        n = n - 1;
    } while (n > 0);

    return n;
}
