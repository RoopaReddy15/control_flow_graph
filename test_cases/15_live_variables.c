// ============================================================
// Test Case 15: Live Variable Analysis (Phase 2)
//
// Verifies: A variable is live at a point if its value may
//   be used in the future before being redefined.
//
// Expected:
//   After "a = 10": a is live (used in c = a+b)
//   After "b = 20": b is live (used in c = a+b)
//   After "unused = 50": unused is NOT live (never read)
//   After "c = a+b": c is live (returned)
// ============================================================

int main() {
    int a = 10;        // a: LIVE (used later)
    int b = 20;        // b: LIVE (used later)
    int unused = 50;   // unused: DEAD (never read → DCE candidate)

    int c = a + b;     // c: LIVE (returned)
                       // a,b: DEAD after this point

    return c;
}
