// ============================================================
// Test Case 22: Multiple Functions (Phase 1)
//
// Verifies: The pipeline processes each user-defined function
//   independently, generating separate CFGs, analysis results,
//   and DOT files for each function.
//
// Expected: Two separate CFGs — one for "add" and one for "main".
//           Each has its own ENTRY/EXIT, analysis, and DOT output.
// ============================================================

int add(int a, int b) {
    return a + b;
}

int main() {
    int x = 10;
    int y = 20;
    int result = add(x, y);
    return result;
}
