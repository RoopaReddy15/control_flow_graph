// ============================================================
// Test Case 06: Switch/Case/Default (Phase 1)
//
// Verifies: switch creates edges from switch block to each
//   case/default block. Break edges go to switch exit.
//   Fall-through occurs when break is missing.
//
// Expected: Multiple edges from switch block, case labels,
//           fall-through from case 2 to case 3 (no break).
// ============================================================

int main() {
    int x = 2;
    int result = 0;

    switch (x) {
        case 1:
            result = 10;
            break;
        case 2:
            result = 20;
            // INTENTIONAL: no break — fall-through to case 3
        case 3:
            result = result + 30;
            break;
        default:
            result = -1;
            break;
    }

    return result;
}
