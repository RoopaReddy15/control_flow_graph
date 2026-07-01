// ============================================================
// main.cpp  —  Driver for the C++ CFG Pipeline
//
// Orchestrates the full compilation pipeline:
//   Phase 1:  Clang AST → CFG construction
//   Phase 2:  Reaching Definitions + Live Variable Analysis
//   Phase 3:  All optimisation passes (iterative)
//   Bonus:    Taint analysis, loop detection, LICM suggestions
//   Output:   Styled DOT files (before.dot / after.dot)
// ============================================================

#include "cfg_builder.h"
#include "dataflow.h"
#include "optimizer.h"
#include "advanced.h"
#include "dot_export.h"

#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/Support/CommandLine.h>

#include <iostream>
#include <iomanip>

using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("cfg-tool options");

// ────────────────────────────────────────────────────────────
// ASTConsumer — invoked once per translation unit
// ────────────────────────────────────────────────────────────

class CFGPipelineConsumer : public ASTConsumer {
public:
    explicit CFGPipelineConsumer(CompilerInstance &CI) : CI(CI) {}

    void HandleTranslationUnit(ASTContext &ctx) override {
        SourceManager &SM = ctx.getSourceManager();

        // Traverse all top-level declarations looking for functions
        for (auto *decl : ctx.getTranslationUnitDecl()->decls()) {
            auto *func = llvm::dyn_cast<FunctionDecl>(decl);
            if (!func || !func->hasBody()) continue;

            // ── Filter: skip functions from system headers ──
            // This mirrors Python's pycparser + fake_libc_include which
            // only processes user-defined functions (not glibc internals
            // like __bswap_16, __uint64_identity, etc.)
            SourceLocation loc = func->getLocation();
            if (SM.isInSystemHeader(loc)) continue;

            // Additional guard: skip compiler built-in functions
            std::string funcName = func->getNameAsString();
            if (funcName.empty()) continue;
            if (funcName.find("__") == 0) continue;  // skip __bswap_*, __uint*_identity, etc.

            std::cout << "\n" << std::string(60, '=') << "\n";
            std::cout << "  Processing function: " << funcName << "\n";
            std::cout << std::string(60, '=') << "\n";

            // ── Phase 1: Build CFG ───────────────────────────
            CFGBuilder builder(funcName);
            CFG cfg = builder.build(func->getBody());

            auto statsBefore = cfg.stats();
            std::cout << "\n[Phase 1] CFG constructed.\n";
            std::cout << "  Blocks: " << statsBefore.blocks
                      << "  Edges: "  << statsBefore.edges
                      << "  Statements: " << statsBefore.statements << "\n";

            // Export BEFORE optimisation
            std::string beforeFile = funcName + "_before.dot";
            DOTExporter::exportToFile(cfg, beforeFile);
            std::cout << "  Exported: " << beforeFile << "\n";

            // ── Phase 2: Static Analysis ─────────────────────
            std::cout << "\n[Phase 2] Static Analysis\n";

            DataFlowAnalysis dfa(cfg);

            // Reaching Definitions
            auto rdResult = dfa.reachingDefinitions();
            std::cout << "  Reaching Definitions computed ("
                      << rdResult.IN.size() << " blocks analysed).\n";

            // Print reaching definitions details (matching Python output)
            for (auto &[bbId, rdIn] : rdResult.IN) {
                auto &rdOut = rdResult.OUT[bbId];
                std::cout << "    RD Block " << bbId << " IN: {";
                bool first = true;
                for (auto &[var, blk, idx] : rdIn) {
                    if (!first) std::cout << ", ";
                    std::cout << var << "@B" << blk << "[" << idx << "]";
                    first = false;
                }
                std::cout << "} OUT: {";
                first = true;
                for (auto &[var, blk, idx] : rdOut) {
                    if (!first) std::cout << ", ";
                    std::cout << var << "@B" << blk << "[" << idx << "]";
                    first = false;
                }
                std::cout << "}\n";
            }

            // Live Variables
            auto lvResult = dfa.liveVariables();
            std::cout << "  Live Variables computed.\n";

            // Print live variable details (both IN and OUT)
            for (auto &[bbId, liveIn] : lvResult.IN) {
                auto &liveOut = lvResult.OUT[bbId];
                std::cout << "    LV Block " << bbId << " IN: {";
                bool first = true;
                for (auto &v : liveIn) {
                    if (!first) std::cout << ", ";
                    std::cout << v;
                    first = false;
                }
                std::cout << "} OUT: {";
                first = true;
                for (auto &v : liveOut) {
                    if (!first) std::cout << ", ";
                    std::cout << v;
                    first = false;
                }
                std::cout << "}\n";
            }

            // ── Uninitialized Variable Detection ─────────────
            // PDF spec use-case: "If a point in the CFG uses x,
            // but no definition of x reaches it, you've found a
            // potential bug!"
            auto uninitWarnings = dfa.detectUninitializedVars();
            if (!uninitWarnings.empty()) {
                std::cout << "\n  Uninitialized Variable Warnings:\n";
                for (auto &w : uninitWarnings)
                    std::cout << "    UNINIT_WARN var='" << w.varName
                              << "' in Block " << w.blockId
                              << " stmt " << w.stmtIndex
                              << " context: " << w.context << "\n";
            } else {
                std::cout << "\n  No uninitialized variable warnings.\n";
            }

            // ── Phase 3: Optimisations ───────────────────────
            std::cout << "\n[Phase 3] Optimizations\n";
            Optimizer opt(cfg);
            opt.optimize();

            for (auto &entry : opt.log)
                std::cout << "  • " << entry << "\n";

            if (opt.log.empty())
                std::cout << "  (no optimizations applied)\n";

            auto statsAfter = cfg.stats();
            std::cout << "\n  Before: " << statsBefore.blocks << " blocks, "
                      << statsBefore.edges << " edges, "
                      << statsBefore.statements << " stmts\n";
            std::cout << "  After:  " << statsAfter.blocks << " blocks, "
                      << statsAfter.edges << " edges, "
                      << statsAfter.statements << " stmts\n";

            // Export AFTER optimisation
            std::string afterFile = funcName + "_after.dot";
            DOTExporter::exportToFile(cfg, afterFile);
            std::cout << "  Exported: " << afterFile << "\n";

            // ── Bonus: Taint Analysis ────────────────────────
            std::cout << "\n[Bonus] Security Scan (Taint Analysis)\n";
            SecurityScanner scanner(cfg);
            auto warnings = scanner.runTaintAnalysis();
            if (warnings.empty()) {
                std::cout << "  No taint warnings.\n";
            } else {
                for (auto &w : warnings)
                    std::cout << "  ⚠ " << w << "\n";
            }

            // Print tainted variables (matching Python output)
            auto &taintedVars = scanner.getTaintedVars();
            if (!taintedVars.empty()) {
                std::cout << "  Tainted Variables:\n";
                for (auto &[var, isTainted] : taintedVars) {
                    std::cout << "    TAINT_VAR " << var << " "
                              << (isTainted ? "TAINTED" : "CLEAN") << "\n";
                }
            }

            // ── Bonus: Loop Detection + LICM ─────────────────
            std::cout << "\n[Bonus] Loop Detection (Dominator Trees)\n";
            LoopDetector detector(cfg);
            auto loops = detector.findLoops();
            if (loops.empty()) {
                std::cout << "  No loops detected.\n";
            } else {
                for (auto &loop : loops) {
                    auto body = detector.getLoopBody(
                        loop.headerId, loop.backNodeId);
                    std::cout << "  Loop: header=Block " << loop.headerId
                              << ", back-edge from Block " << loop.backNodeId
                              << ", body={";
                    bool first = true;
                    for (int b : body) {
                        if (!first) std::cout << ", ";
                        std::cout << b;
                        first = false;
                    }
                    std::cout << "}\n";
                }
            }

            // Print dominator sets (matching Python output)
            auto &doms = detector.getDominators();
            if (!doms.empty()) {
                std::cout << "  Dominator Sets:\n";
                for (auto &[nodeId, domSet] : doms) {
                    std::cout << "    DOM Block " << nodeId << " dominated_by: {";
                    bool first = true;
                    for (int d : domSet) {
                        if (!first) std::cout << ", ";
                        std::cout << d;
                        first = false;
                    }
                    std::cout << "}\n";
                }
            }

            // LICM
            auto licm = detector.suggestLICM();
            if (!licm.empty()) {
                std::cout << "\n  LICM Candidates:\n";
                for (auto &c : licm)
                    std::cout << "    • LICM " << c.description << "\n";
            }

            std::cout << "\n";
        }
    }

private:
    CompilerInstance &CI;
};

// ────────────────────────────────────────────────────────────
// FrontendAction
// ────────────────────────────────────────────────────────────

class CFGPipelineAction : public ASTFrontendAction {
public:
    std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance &CI, StringRef) override {
        return std::make_unique<CFGPipelineConsumer>(CI);
    }
};

// ────────────────────────────────────────────────────────────
// MAIN
// ────────────────────────────────────────────────────────────

int main(int argc, const char **argv) {
    auto expected = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!expected) {
        llvm::errs() << expected.takeError() << "\n";
        return 1;
    }

    auto &parser = expected.get();
    ClangTool tool(parser.getCompilations(), parser.getSourcePathList());

    std::cout << "========================================================\n";
    std::cout << "|  C Code -> Optimized CFG Pipeline  (C++ Edition)     |\n";
    std::cout << "|  Phase 1: CFG Construction (Clang AST)               |\n";
    std::cout << "|  Phase 2: Reaching Defs + Live Variables             |\n";
    std::cout << "|  Phase 3: Constant Fold/Prop, DCE, Unreachable       |\n";
    std::cout << "|  Bonus:   Taint Analysis, Loop Detection, LICM       |\n";
    std::cout << "========================================================\n";

    return tool.run(newFrontendActionFactory<CFGPipelineAction>().get());
}
