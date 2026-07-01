// ============================================================
// Test Case 12: Unreachable Code Removal (Phase 3)
//
// Verifies: Blocks with no incoming path from the entry node
//   are deleted from the CFG.
//   PDF spec: "Removing CFG nodes that have no incoming path
//   from the 'Start' node. Simple Graph Traversal (DFS/BFS)."
//
// Expected: Code after return and after goto is removed.
//           Branch pruning + unreachable removal together
//           eliminate the dead-if branch entirely.
// ============================================================

int main() {
    int x = 0;

    // Unreachable after return
    if (x == 0) {
        return 42;
    }
    // This code is reachable (x might not be 0... but after
    // propagation x=0, so if(0==0) is always true, the else
    // path becomes unreachable)

    int never_reached = 999;  // unreachable after branch pruning
    return never_reached;
}
