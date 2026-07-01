#pragma once
// ============================================================
// advanced.h  —  Bonus Optimizations
//
// Mirrors Python's analysis/advanced.py.
// Implements all bonus features from pdf_content.txt:
//   1.  SecurityScanner  — Taint Analysis (sources → sinks)
//   2.  LoopDetector     — Dominator Trees + back-edge detection
//   3.  LICM suggestions — Loop-Invariant Code Motion candidates
// ============================================================

#include "cfg.h"
#include "dataflow.h"

#include <set>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <algorithm>

// ────────────────────────────────────────────────────────────
// SecurityScanner  — Taint Analysis
// ────────────────────────────────────────────────────────────

class SecurityScanner {
public:
    explicit SecurityScanner(const CFG &cfg) : cfg(cfg) {}

    std::vector<std::string> runTaintAnalysis() {
        // Sources: functions that introduce untrusted user input
        std::set<std::string> sources = {
            "scanf", "gets", "read", "fgets", "getchar", "getenv"
        };
        // Sinks: functions where tainted data is dangerous
        std::set<std::string> sinks = {
            "system", "printf", "exec", "execve", "popen", "sprintf"
        };

        // BFS traversal from entry (forward order)
        std::queue<int> q;
        std::set<int> visited;
        q.push(cfg.entryId);

        while (!q.empty()) {
            int id = q.front(); q.pop();
            if (visited.count(id)) continue;
            if (cfg.blocks.find(id) == cfg.blocks.end()) continue;
            visited.insert(id);

            const auto &bb = cfg.blocks.at(id);
            for (const auto &inst : bb.instructions)
                processStatement(inst, sources, sinks);

            for (int succ : bb.successors)
                q.push(succ);
        }

        return warnings;
    }

    const std::map<std::string, bool> &getTaintedVars() const {
        return taintedVars;
    }

private:
    const CFG &cfg;
    std::map<std::string, bool> taintedVars;
    std::vector<std::string> warnings;

    void processStatement(const Instruction &inst,
                          const std::set<std::string> &sources,
                          const std::set<std::string> &sinks) {
        // ── Standalone function call ─────────────────────────
        if (inst.kind == Instruction::FUNC_CALL_STMT) {
            if (sources.count(inst.funcName)) {
                markSourceArgsTainted(inst);
            } else if (sinks.count(inst.funcName)) {
                checkSinkArgs(inst);
            }
            return;
        }

        // ── Assignment (x = expr) ────────────────────────────
        if (inst.kind == Instruction::ASSIGN ||
            inst.kind == Instruction::DECL_ASSIGN) {
            if (inst.lhs.empty()) return;

            // Check if RHS is a call to a taint source
            if (inst.rhs && inst.rhs->isCall() &&
                sources.count(inst.rhs->funcName)) {
                taintedVars[inst.lhs] = true;
                return;
            }

            // Propagate: if any used variable is tainted → lhs tainted
            auto uses = inst.getUses();
            bool isTainted = false;
            for (auto &u : uses) {
                if (taintedVars.count(u) && taintedVars[u]) {
                    isTainted = true;
                    break;
                }
            }
            taintedVars[inst.lhs] = isTainted;
        }
    }

    // Mark variables passed to taint sources as tainted.
    // Handles: scanf("%d", &x) → x is tainted
    //          gets(buf) → buf is tainted
    void markSourceArgsTainted(const Instruction &inst) {
        for (auto &arg : inst.callArgs) {
            if (!arg) continue;

            // Address-of pattern: scanf("%d", &x) → x tainted
            if (arg->kind == Expr::UNARYOP && arg->op == "&") {
                if (arg->operand && arg->operand->isVar()) {
                    taintedVars[arg->operand->varName] = true;
                }
            }
            // Direct variable: gets(buf) → buf tainted
            else if (arg->isVar()) {
                taintedVars[arg->varName] = true;
            }
        }
    }

    // Check if tainted data flows into sink arguments.
    void checkSinkArgs(const Instruction &inst) {
        for (auto &arg : inst.callArgs) {
            if (!arg) continue;
            // Skip string literal constants (no taint)
            if (arg->kind == Expr::UNKNOWN &&
                !arg->raw.empty() && arg->raw[0] == '"')
                continue;

            std::set<std::string> uses;
            arg->getUsedVars(uses);
            for (auto &u : uses) {
                if (taintedVars.count(u) && taintedVars[u]) {
                    warnings.push_back(
                        "Tainted variable '" + u +
                        "' reached sink '" + inst.funcName + "'");
                }
            }
        }
    }
};

// ────────────────────────────────────────────────────────────
// LoopDetector  — Dominator Trees + Natural Loop Detection
// ────────────────────────────────────────────────────────────

class LoopDetector {
public:
    explicit LoopDetector(const CFG &cfg) : cfg(cfg) {}

    // ── Compute dominator sets ───────────────────────────────
    // Algorithm: iterative fixed-point (Allen & Cocke, 1970)
    //   Dom(entry) = {entry}
    //   Dom(N) = {N} ∪ (∩ Dom(P) for all predecessors P)
    void computeDominators() {
        computeReachable();

        std::vector<int> nodes;
        for (auto &[id, _] : cfg.blocks)
            if (reachable.count(id))
                nodes.push_back(id);

        if (nodes.empty()) return;

        // Initialize: every reachable node dominated by all
        std::set<int> allSet(nodes.begin(), nodes.end());
        for (int n : nodes)
            dominators[n] = allSet;
        dominators[cfg.entryId] = {cfg.entryId};

        bool changed = true;
        while (changed) {
            changed = false;
            for (int n : nodes) {
                if (n == cfg.entryId) continue;

                const auto &bb = cfg.blocks.at(n);
                // Only consider reachable predecessors
                std::vector<int> reachablePreds;
                for (int p : bb.predecessors)
                    if (reachable.count(p))
                        reachablePreds.push_back(p);

                std::set<int> newDom;
                if (!reachablePreds.empty()) {
                    newDom = allSet;
                    for (int p : reachablePreds) {
                        std::set<int> inter;
                        std::set_intersection(
                            newDom.begin(), newDom.end(),
                            dominators[p].begin(), dominators[p].end(),
                            std::inserter(inter, inter.begin()));
                        newDom = inter;
                    }
                }
                newDom.insert(n);

                if (newDom != dominators[n]) {
                    dominators[n] = newDom;
                    changed = true;
                }
            }
        }
    }

    // ── Find natural loops via back-edges ────────────────────
    // A back-edge is N → H where H dominates N.
    struct Loop {
        int headerId;
        int backNodeId;
    };

    std::vector<Loop> findLoops() {
        computeDominators();
        std::vector<Loop> loops;

        for (auto &[n, bb] : cfg.blocks) {
            if (!reachable.count(n)) continue;
            for (int succ : bb.successors) {
                if (reachable.count(succ) &&
                    dominators.count(n) &&
                    dominators[n].count(succ)) {
                    loops.push_back({succ, n});
                }
            }
        }
        return loops;
    }

    // ── Compute natural loop body ────────────────────────────
    // All nodes that can reach the back-edge source without
    // going through the header.
    std::set<int> getLoopBody(int headerId, int backNodeId) {
        std::set<int> body = {headerId};
        if (headerId == backNodeId) return body; // self-loop

        std::vector<int> worklist = {backNodeId};
        while (!worklist.empty()) {
            int m = worklist.back(); worklist.pop_back();
            if (body.count(m)) continue;
            body.insert(m);
            auto it = cfg.blocks.find(m);
            if (it != cfg.blocks.end()) {
                for (int pred : it->second.predecessors)
                    if (!body.count(pred))
                        worklist.push_back(pred);
            }
        }
        return body;
    }

    // ── LICM Suggestions ─────────────────────────────────────
    // A statement is loop-invariant if ALL its used variables are
    // defined OUTSIDE the loop body.
    struct LICMCandidate {
        int headerId, backNodeId;
        int blockId;
        int stmtIndex;
        std::string variable;
        std::string description;
    };

    std::vector<LICMCandidate> suggestLICM() {
        auto loops = findLoops();
        std::vector<LICMCandidate> candidates;

        for (auto &loop : loops) {
            auto body = getLoopBody(loop.headerId, loop.backNodeId);

            // Gather all variables defined inside the loop
            std::set<std::string> loopDefs;
            for (int bid : body) {
                auto it = cfg.blocks.find(bid);
                if (it == cfg.blocks.end()) continue;
                for (auto &inst : it->second.instructions) {
                    auto defs = inst.getDefs();
                    loopDefs.insert(defs.begin(), defs.end());
                }
            }

            // Scan for invariant assignments
            for (int bid : body) {
                auto it = cfg.blocks.find(bid);
                if (it == cfg.blocks.end() || bid == loop.headerId) continue;

                for (int idx = 0; idx < (int)it->second.instructions.size(); idx++) {
                    auto &inst = it->second.instructions[idx];
                    if (inst.kind != Instruction::ASSIGN &&
                        inst.kind != Instruction::DECL_ASSIGN)
                        continue;
                    if (inst.lhs.empty() || !inst.rhs) continue;

                    std::set<std::string> uses;
                    inst.rhs->getUsedVars(uses);

                    // Must have uses (not a bare constant) and all external
                    if (!uses.empty() &&
                        std::all_of(uses.begin(), uses.end(),
                                    [&](const std::string &u) {
                                        return !loopDefs.count(u);
                                    })) {
                        LICMCandidate c;
                        c.headerId = loop.headerId;
                        c.backNodeId = loop.backNodeId;
                        c.blockId = bid;
                        c.stmtIndex = idx;
                        c.variable = inst.lhs;
                        c.description =
                            "Assignment to '" + inst.lhs + "' in Block " +
                            std::to_string(bid) +
                            " uses only loop-external variables — "
                            "candidate for LICM (move before loop header "
                            "Block " + std::to_string(loop.headerId) + ")";
                        candidates.push_back(c);
                    }
                }
            }
        }
        return candidates;
    }

    const std::map<int, std::set<int>> &getDominators() const {
        return dominators;
    }

private:
    const CFG &cfg;
    std::set<int> reachable;
    std::map<int, std::set<int>> dominators;

    void computeReachable() {
        reachable.clear();
        std::queue<int> q;
        q.push(cfg.entryId);
        while (!q.empty()) {
            int id = q.front(); q.pop();
            if (reachable.count(id)) continue;
            if (cfg.blocks.find(id) == cfg.blocks.end()) continue;
            reachable.insert(id);
            for (int succ : cfg.blocks.at(id).successors)
                q.push(succ);
        }
    }
};
