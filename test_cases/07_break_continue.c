// ============================================================
// Test Case 07: Break and Continue (Phase 1)
//
// Verifies: break jumps to loop exit, continue jumps to header.
//   Both create "dead blocks" for code after them.
//
// Expected: break edge → exit block, continue edge → header,
//           dead blocks after break/continue are unreachable.
// ============================================================

int main() {
    int i = 0;
    int sum = 0;

    while (i < 100) {
        i = i + 1;

        if (i == 5) {
            continue;   // skip rest, go to header
        }

        if (i == 10) {
            break;       // exit loop entirely
        }

        sum = sum + i;
    }

    return sum;
}
