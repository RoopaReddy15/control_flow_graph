// ============================================================
// Test Case 02: If/Else Branching (Phase 1)
//
// Verifies: if/else creates a diamond CFG pattern.
//   ENTRY → cond_block → (then_block | else_block) → merge → EXIT
//
// Expected: True/False edge labels, merge block after branches.
// ============================================================

int main() {
    int x = 10;
    int result;

    if (x > 5) {
        result = 1;
    } else {
        result = 0;
    }

    return result;
}
