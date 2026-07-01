// ============================================================
// Test Case 19: LICM — Loop-Invariant Code Motion (Bonus)
//
// Verifies: Assignments inside a loop that use only variables
//   defined OUTSIDE the loop are identified as LICM candidates.
//   PDF spec: "moving calculations that don't change inside a
//   loop to just before the loop starts"
//
// Expected LICM candidates:
//   - "factor = base + offset" (base and offset defined outside)
//   - NOT "total = total + factor" (total changes each iteration)
//   - NOT "i = i + 1" (i changes each iteration)
// ============================================================

int main() {
    int base = 10;
    int offset = 5;
    int total = 0;

    int i = 0;
    while (i < 100) {
        int factor = base + offset;  // LICM CANDIDATE: invariant
        total = total + factor;       // NOT invariant (total changes)
        i = i + 1;                    // NOT invariant (i changes)
    }

    return total;
}
