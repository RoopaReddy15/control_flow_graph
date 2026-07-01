// ============================================================
// Test Case 01: Basic CFG Construction (Phase 1)
//
// Verifies: Sequential code produces a linear chain of blocks
//           ENTRY → body → EXIT with correct edges.
//
// Expected: 3 blocks (ENTRY, body, EXIT), 2 edges,
//           all assignments appear as instructions in body block.
// ============================================================

int main() {
    int a = 1;
    int b = 2;
    int c = a + b;
    return c;
}
