#include <stdio.h>
#include <stdlib.h>

int main() {
    int debug_mode = 0;
    int a = 10;
    int b = 20;
    
    // Constant propagation & folding target
    int unused_calc = a * b * 50; 
    
    // Taint analysis target
    char user_input[100];
    scanf("%99s", user_input); 
    
    /* 
     * Because debug_mode = 0 above, the optimizer will propagate '0' into this condition.
     * The Branch Pruner will snip the 'True' Path edge.
     * The Unreachable Code pass will then permanently delete the printf block out of the Optimized CFG!
     */
    if (debug_mode) {
        printf("This branch is unreachable and will be Erased structually!");
        a = 999;
    } else {
        b = a + b;
    }
    
    // Security Sink Warning
    system(user_input);
    
    return b;
}
