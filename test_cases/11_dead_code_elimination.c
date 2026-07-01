// ============================================================
// Test Case 11: Dead Code Elimination (Phase 3)
//
// Verifies: Assignments to variables that are never read
//   (not "live") are removed by DCE using Live Variable Analysis.
//   PDF spec: "If a variable is assigned but is never 'live'
//   again before being reassigned, that assignment is useless."
//
// Expected: dead1, dead2, dead3 assignments removed.
//           keep1 and result survive (they are used).
// ============================================================

int main() {
    int dead1 = 42;         // DEAD: never used
    int dead2 = 100;        // DEAD: overwritten before use
    dead2 = 200;            // DEAD: still never used

    int keep1 = 10;         // LIVE: used in result

    int dead3 = keep1 * 5;  // DEAD: never used

    int result = keep1 + 1; // LIVE: returned

    return result;
}
