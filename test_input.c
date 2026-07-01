// test_input.c — Exercises all pipeline features
//
//   Phase 1: if/else, while, for, do-while, switch, break,
//            continue, return, goto/label
//   Phase 2: Variables with reaching defs and liveness
//   Phase 3: Constant folding, propagation, dead code, unreachable
//   Bonus:   Taint source (scanf), taint sink (system, printf),
//            loop with LICM candidate

#include <stdio.h>
#include <stdlib.h>

int main() {
    // ── Constant folding + propagation ──────────────────
    int x = 3 + 5;             // folds to x = 8
    int y = x * 2;             // propagates: y = 8 * 2 = 16
    int z = 100;               // dead code (z never used)

    // ── Taint source ────────────────────────────────────
    int user_input;
    scanf("%d", &user_input);  // user_input is tainted

    // ── If / Else ───────────────────────────────────────
    if (x > 0) {
        printf("x is positive: %d\n", x);
    } else {
        printf("x is non-positive\n");
    }

    // ── While loop ──────────────────────────────────────
    int count = 0;
    while (count < y) {
        count = count + 1;
    }

    // ── For loop with LICM candidate ────────────────────
    int total = 0;
    int multiplier = 10;
    for (int i = 0; i < 5; i++) {
        int factor = multiplier + x;  // LICM: uses only outer vars
        total = total + factor;
    }

    // ── Do-While ────────────────────────────────────────
    int d = 3;
    do {
        d = d - 1;
    } while (d > 0);

    // ── Switch ──────────────────────────────────────────
    switch (x) {
        case 8:
            printf("x is 8\n");
            break;
        case 10:
            printf("x is 10\n");
            break;
        default:
            printf("x is something else\n");
            break;
    }

    // ── Taint propagation → sink ────────────────────────
    int cmd_val = user_input + 50;  // taint propagates
    // system() with tainted argument (security warning!)
    char buf[64];
    sprintf(buf, "echo %d", cmd_val);
    system(buf);

    // ── Goto / Label ────────────────────────────────────
    goto skip;
    int unreachable_var = 999;  // unreachable code
skip:
    printf("After goto\n");

    return total + count;
}
