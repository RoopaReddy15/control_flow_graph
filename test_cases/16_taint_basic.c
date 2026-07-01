// ============================================================
// Test Case 16: Taint Analysis — Basic Flow (Bonus: Security)
//
// Verifies: User input from scanf is tainted and tracked
//   through assignments until it reaches a sink (system).
//   PDF spec: "Mark user input (like scanf) as 'tainted' and
//   see if tainted data can reach a 'sink' (like system())"
//
// Expected warnings:
//   - user_val tainted via scanf
//   - computed tainted (uses user_val)
//   - system(computed) → TAINT WARNING
// ============================================================

#include <stdio.h>
#include <stdlib.h>

int main() {
    int user_val;
    scanf("%d", &user_val);     // SOURCE: user_val tainted

    int computed = user_val * 2; // PROPAGATION: computed tainted

    system("echo hello");       // SAFE: literal string arg, no taint

    // Tainted data reaches dangerous sink
    char cmd[64];
    sprintf(cmd, "%d", computed);  // WARNING: tainted 'computed' → sprintf
    system(cmd);                   // WARNING: tainted 'cmd' → system

    return 0;
}
