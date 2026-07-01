// ============================================================
// Test Case 20: Uninitialized Variable Detection (Phase 2)
//
// Verifies: When a variable is used but no definition reaches
//   that point, a warning is generated.
//   PDF spec: "If a point in the CFG uses x, but no definition
//   of x reaches it, you've found a potential bug!"
//
// Expected warnings:
//   - 'uninit_var' used without initialization
//   - 'maybe_uninit' used — only defined in one branch of if/else
//     (the reaching def may not cover all paths)
// ============================================================

int main() {
    int uninit_var;            // declared but never initialized

    int result = uninit_var;   // BUG: uses uninit_var

    return result;
}
