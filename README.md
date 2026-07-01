# C Code → Optimized CFG Pipeline (C++ Edition)

A complete C++ implementation of the compiler pipeline using **LLVM/Clang** (Option A from the project specification).

## Architecture

```
cpp_impl/
├── ir.h                       Phase 1  Expression + Instruction IR types
├── cfg.h                      Phase 1  BasicBlock + CFG data structures
├── cfg_builder.h              Phase 1  Clang AST → CFG (all C constructs)
├── dataflow.h                 Phase 2  Reaching Definitions + Live Variables
├── optimizer.h                Phase 3  6 optimization passes (iterative)
├── advanced.h                 Bonus    Taint Analysis + Loop Detection + LICM
├── dot_export.h               Output   Styled Graphviz DOT export
├── main.cpp                   Driver   Orchestrates full pipeline
├── cpp_app.py                 UI       Streamlit web dashboard
├── CMakeLists.txt             Build    CMake build configuration
├── test_input.c               Test     Comprehensive test file
├── project_documentation.txt  Docs     Full project documentation
└── README.md                  Docs     This file
```

## Features Covered

### Phase 1: C → CFG (The Foundation)
- Uses Clang AST API (RecursiveASTVisitor header for function discovery, manual dispatch for CFG construction — Option A)
- Handles: if/else, while, for, do-while, switch/case/default, break, continue, return, goto/label
- Proper ENTRY and EXIT blocks
- Break/continue target stacks for nested loops

### Phase 2: Static Analysis
- **Reaching Definitions Analysis** — forward, iterative fixed-point
- **Live Variable Analysis** — backward, iterative fixed-point
- **Uninitialized Variable Detection** — uses reaching definitions to find uses without prior definitions
- Proper DEF/USE extraction from structured IR

### Phase 3: Optimizations
- **Constant Folding** — all 17+ operators (arithmetic, relational, logical, bitwise, unary)
- **Constant Propagation** — intra-block, replaces variables with known constants
- **Branch Pruning** — removes dead edges when conditions fold to constants
- **Dead Code Elimination** — uses live variable analysis
- **Unreachable Code Removal** — BFS from entry
- **Empty Block Merging** — rewires predecessors to successor
- All passes run **iteratively** until fixed-point (max 20 iterations)

### Bonus Features
- **Taint Analysis** — tracks scanf/gets/read → system/printf/exec flows
- **Loop Detection** — dominator tree computation + back-edge identification
- **LICM Suggestions** — identifies loop-invariant assignments

## Prerequisites

- LLVM/Clang development libraries (14+ recommended)
- CMake 3.14+
- C++17 compiler

### Installing LLVM on Windows
```powershell
# Option 1: Chocolatey
choco install llvm

# Option 2: Download from https://releases.llvm.org/
# Install the LLVM development package with headers and CMake configs
```

### Installing LLVM on Ubuntu/Debian
```bash
sudo apt install llvm-dev libclang-dev clang
```

## Build Instructions

```bash
# From the cpp_impl directory:
mkdir build && cd build
cmake .. -DLLVM_DIR="<path-to-llvm>/lib/cmake/llvm" \
         -DClang_DIR="<path-to-clang>/lib/cmake/clang"
cmake --build .
```

## Usage

```bash
# Run on the test file:
./cfg_tool test_input.c --

# Run on any C file:
./cfg_tool <your_file.c> --
```

The `--` separator tells Clang Tooling there are no extra compiler flags.

## Output

The tool produces:
1. **Console output** — detailed analysis results for each function
2. **`<func>_before.dot`** — CFG before optimization (Graphviz DOT)
3. **`<func>_after.dot`** — CFG after optimization (Graphviz DOT)

Render DOT files with:
```bash
dot -Tpng main_before.dot -o main_before.png
dot -Tpng main_after.dot  -o main_after.png
```

## Web Dashboard

```bash
streamlit run cpp_app.py
```

Paste C code on the left → see Original CFG, Optimized CFG, analysis results, and security warnings on the right.

## Test Cases

25 test cases covering every feature from the project specification:

| # | File | Feature Tested |
|---|------|---------------|
| 01 | `01_basic_cfg.c` | Linear CFG construction |
| 02 | `02_if_else.c` | If/else diamond pattern |
| 03 | `03_while_loop.c` | While loop + loop detection |
| 04 | `04_for_loop.c` | For loop structure |
| 05 | `05_do_while.c` | Do-while loop |
| 06 | `06_switch_case.c` | Switch/case/default + fall-through |
| 07 | `07_break_continue.c` | Break and continue |
| 08 | `08_goto_label.c` | Forward/backward goto |
| 09 | `09_constant_folding.c` | All folding operators |
| 10 | `10_constant_propagation.c` | Propagation + branch pruning cascade |
| 11 | `11_dead_code_elimination.c` | Dead assignment removal |
| 12 | `12_unreachable_code.c` | Unreachable block removal |
| 13 | `13_branch_pruning.c` | Constant condition pruning |
| 14 | `14_reaching_definitions.c` | Multiple reaching defs via branches |
| 15 | `15_live_variables.c` | Live vs dead variables |
| 16 | `16_taint_basic.c` | Taint source → sink flow |
| 17 | `17_taint_multiple.c` | Multiple sources and sinks |
| 18 | `18_nested_loops.c` | Nested loop dominator detection |
| 19 | `19_licm.c` | Loop-invariant code motion candidates |
| 20 | `20_uninitialized_var.c` | Uninitialized variable detection |
| 21 | `21_empty_block_merging.c` | Empty block merging |
| 22 | `22_multiple_functions.c` | Multi-function CFG generation |
| 23 | `23_optimization_cascade.c` | Iterative optimization interaction |
| 24 | `24_compound_assignments.c` | +=, -=, *=, /=, %= operators |
| 25 | `25_full_pipeline_stress.c` | All features combined stress test |

## Documentation

- `project_documentation.txt` — Complete architecture, data flow, algorithms, file descriptions
