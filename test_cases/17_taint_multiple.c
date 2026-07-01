// ============================================================
// Test Case 17: Taint Analysis — Multiple Sources & Sinks
//
// Verifies: Multiple taint sources and sinks, including
//   gets(), getenv(), printf(), popen(), exec().
//
// Expected: Multiple taint warnings for each tainted flow.
// ============================================================

#include <stdio.h>
#include <stdlib.h>

int main() {
    // Source 1: scanf
    int x;
    scanf("%d", &x);           // x tainted

    // Source 2: getenv
    char *path = getenv("PATH"); // path tainted

    // Clean variable
    int safe = 42;

    // Tainted flows to sinks
    printf("%d\n", x);         // WARNING: x → printf
    printf("safe: %d\n", safe); // SAFE: safe is clean

    // Tainted propagation chain
    int derived = x + safe;     // derived tainted (uses x)
    printf("%d\n", derived);    // WARNING: derived → printf

    return 0;
}
