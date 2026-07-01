#pragma once
// ============================================================
// optimizer.h  —  Optimization Passes (Phase 3)
//
// Mirrors Python's optimization/optimizer.py.
// Implements ALL optimizations from pdf_content.txt:
//   1.  Constant Folding     (all arithmetic/relational/logical ops)
//   2.  Constant Propagation (intra-block, into conditions)
//   3.  Branch Pruning       (constant conditions → prune dead edges)
//   4.  Dead Code Elimination (using live variable analysis)
//   5.  Unreachable Code Removal  (BFS from entry)
//   6.  Empty Block Merging  (rewire predecessors → successor)
//
// Runs all passes ITERATIVELY until fixed-point (max 20 iters).
// ============================================================

#include "cfg.h"
#include "dataflow.h"

#include <vector>
#include <map>
#include <set>
#include <string>
#include <queue>
#include <algorithm>

class Optimizer {
public:
    CFG &cfg;
    std::vector<std::string> log; // optimisation log

    explicit Optimizer(CFG &c) : cfg(c) {}

    // ── Run all passes iteratively ───────────────────────────
    void optimize() {
        for (int iter = 0; iter < 20; iter++) {
            bool c1 = constantFolding();
            bool c2 = constantPropagation();
            bool c3 = pruneConstantBranches();
            bool c4 = eliminateUnreachable();
            bool c5 = deadCodeElimination();
            bool c6 = mergeEmptyBlocks();
            if (!(c1 || c2 || c3 || c4 || c5 || c6)) break;
            if (iter == 19)
                log.push_back("Safety limit reached (20 iterations)");
        }
    }

    // ── 1. Constant Folding ──────────────────────────────────
    // Recursively folds constant sub-expressions in all instructions.
    // Supports: +, -, *, /, %, <, >, <=, >=, ==, !=, &&, ||
    // Unary:    -, !, ~
    bool constantFolding() {
        bool changed = false;
        for (auto &[bbId, bb] : cfg.blocks) {
            for (auto &inst : bb.instructions) {
                ExprPtr *target = nullptr;
                if (inst.kind == Instruction::ASSIGN ||
                    inst.kind == Instruction::DECL_ASSIGN)
                    target = &inst.rhs;
                else if (inst.kind == Instruction::COND)
                    target = &inst.cond;
                else if (inst.kind == Instruction::RETURN_STMT)
                    target = &inst.retExpr;

                if (target && *target) {
                    auto folded = foldExpr(*target);
                    if (folded.get() != target->get()) {
                        *target = folded;
                        changed = true;
                    }
                }
            }
        }
        return changed;
    }

    // ── 2. Constant Propagation (intra-block) ────────────────
    // Tracks which variables have known constant values within
    // a block and replaces variable references with constants.
    bool constantPropagation() {
        bool changed = false;
        for (auto &[bbId, bb] : cfg.blocks) {
            std::map<std::string, int> constants;

            for (auto &inst : bb.instructions) {
                // Propagate known constants into expressions
                ExprPtr *target = nullptr;
                if (inst.kind == Instruction::ASSIGN ||
                    inst.kind == Instruction::DECL_ASSIGN)
                    target = &inst.rhs;
                else if (inst.kind == Instruction::COND)
                    target = &inst.cond;

                if (target && *target) {
                    if (propagateInExpr(*target, constants)) {
                        changed = true;
                        if (inst.kind == Instruction::COND) {
                            log.push_back("Propagated constant into " +
                                          inst.condTag + " in Block " +
                                          std::to_string(bbId));
                        }
                    }
                }

                // Track constant assignments
                if ((inst.kind == Instruction::ASSIGN ||
                     inst.kind == Instruction::DECL_ASSIGN) && inst.rhs) {
                    if (inst.rhs->isConst()) {
                        constants[inst.lhs] = inst.rhs->intVal;
                    } else {
                        constants.erase(inst.lhs); // no longer constant
                    }
                }
            }
        }
        return changed;
    }

    // ── 3. Branch Pruning ────────────────────────────────────
    // If a condition has been folded to a constant, remove
    // the dead branch edge.
    bool pruneConstantBranches() {
        bool changed = false;
        for (auto &[bbId, bb] : cfg.blocks) {
            for (size_t idx = 0; idx < bb.instructions.size(); idx++) {
                auto &inst = bb.instructions[idx];
                if (inst.kind != Instruction::COND) continue;
                if (!inst.cond || !inst.cond->isConst()) continue;
                if (bb.successors.size() != 2) continue;

                int trueBranch  = bb.successors[0];
                int falseBranch = bb.successors[1];
                int val = inst.cond->intVal;

                if (val == 0) {
                    // Condition false → remove true branch
                    removeEdge(bbId, trueBranch);
                    inst = Instruction::makeBranchPruned();
                    log.push_back("Pruned TRUE branch from Block " +
                                  std::to_string(bbId) + " (condition=0)");
                } else {
                    // Condition true → remove false branch
                    removeEdge(bbId, falseBranch);
                    inst = Instruction::makeBranchPruned();
                    log.push_back("Pruned FALSE branch from Block " +
                                  std::to_string(bbId) + " (condition=" +
                                  std::to_string(val) + ")");
                }
                changed = true;
            }
        }
        return changed;
    }

    // ── 4. Unreachable Code Removal ──────────────────────────
    // BFS from entry; any block not reached is deleted.
    bool eliminateUnreachable() {
        std::set<int> reachable;
        std::queue<int> q;
        q.push(cfg.entryId);

        while (!q.empty()) {
            int id = q.front(); q.pop();
            if (reachable.count(id)) continue;
            if (cfg.blocks.find(id) == cfg.blocks.end()) continue;
            reachable.insert(id);
            for (int succ : cfg.blocks[id].successors)
                q.push(succ);
        }

        std::vector<int> toRemove;
        for (auto &[id, _] : cfg.blocks)
            if (!reachable.count(id))
                toRemove.push_back(id);

        if (toRemove.empty()) return false;

        log.push_back("Removed " + std::to_string(toRemove.size()) +
                       " unreachable block(s)");
        for (int id : toRemove)
            cfg.removeBlock(id);
        return true;
    }

    // ── 5. Dead Code Elimination ─────────────────────────────
    // Uses live variable analysis: if a variable is assigned
    // but not live-out, the assignment is dead.
    bool deadCodeElimination() {
        DataFlowAnalysis dfa(cfg);
        auto liveResult = dfa.liveVariables();
        bool changed = false;

        for (auto &[bbId, bb] : cfg.blocks) {
            auto live = liveResult.OUT[bbId];
            std::vector<Instruction> newInsts;

            // Walk instructions in reverse (like the Python version)
            for (int i = (int)bb.instructions.size() - 1; i >= 0; i--) {
                auto &inst = bb.instructions[i];
                auto defs = inst.getDefs();
                auto uses = inst.getUses();

                // Check if it's a dead assignment
                if ((inst.kind == Instruction::ASSIGN ||
                     inst.kind == Instruction::DECL_ASSIGN) &&
                    !inst.lhs.empty()) {
                    if (live.find(inst.lhs) == live.end()) {
                        // Also skip if rhs is a function call (side effects)
                        bool hasSideEffect = inst.rhs && inst.rhs->isCall();
                        if (!hasSideEffect) {
                            changed = true;
                            log.push_back("Eliminated dead assignment to '" +
                                          inst.lhs + "' in Block " +
                                          std::to_string(bbId));
                            // Update live set: remove defs, add nothing
                            for (auto &d : defs) live.erase(d);
                            continue; // skip this instruction
                        }
                    }
                }

                // Update live set
                for (auto &d : defs) live.erase(d);
                live.insert(uses.begin(), uses.end());
                newInsts.insert(newInsts.begin(), inst);
            }

            bb.instructions = newInsts;
        }
        return changed;
    }

    // ── 6. Empty Block Merging ───────────────────────────────
    // Eliminates blocks with no real statements by rewiring
    // predecessors to point directly to the successor.
    bool mergeEmptyBlocks() {
        bool changed = false;

        for (auto it = cfg.blocks.begin(); it != cfg.blocks.end(); ) {
            int bbId = it->first;
            auto &bb = it->second;

            if (bbId == cfg.entryId || bbId == cfg.exitId) {
                ++it; continue;
            }

            // Count real statements (ignore BRANCH_PRUNED)
            int realStmts = 0;
            for (auto &inst : bb.instructions)
                if (inst.kind != Instruction::BRANCH_PRUNED)
                    realStmts++;

            if (realStmts == 0 && !bb.predecessors.empty() &&
                bb.successors.size() == 1) {
                int succId = bb.successors[0];
                auto &succ = cfg.blocks[succId];

                // Redirect predecessors
                for (int predId : bb.predecessors) {
                    auto &pred = cfg.blocks[predId];
                    std::replace(pred.successors.begin(),
                                 pred.successors.end(), bbId, succId);
                    if (std::find(succ.predecessors.begin(),
                                  succ.predecessors.end(), predId)
                            == succ.predecessors.end())
                        succ.predecessors.push_back(predId);
                }

                // Remove bb from successor's predecessors
                succ.predecessors.erase(
                    std::remove(succ.predecessors.begin(),
                                succ.predecessors.end(), bbId),
                    succ.predecessors.end());

                log.push_back("Merged empty Block " + std::to_string(bbId) +
                               " into Block " + std::to_string(succId));
                it = cfg.blocks.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
        return changed;
    }

private:
    // ── Recursive expression folder ──────────────────────────
    ExprPtr foldExpr(ExprPtr e) {
        if (!e) return e;

        if (e->kind == Expr::BINOP) {
            auto l = foldExpr(e->left);
            auto r = foldExpr(e->right);

            if (l->isConst() && r->isConst()) {
                int lv = l->intVal, rv = r->intVal;
                int result = 0;
                bool valid = true;

                if      (e->op == "+")  result = lv + rv;
                else if (e->op == "-")  result = lv - rv;
                else if (e->op == "*")  result = lv * rv;
                else if (e->op == "/")  { if (rv == 0) valid = false; else result = lv / rv; }
                else if (e->op == "%")  { if (rv == 0) valid = false; else result = lv % rv; }
                else if (e->op == "<")  result = (int)(lv < rv);
                else if (e->op == ">")  result = (int)(lv > rv);
                else if (e->op == "<=") result = (int)(lv <= rv);
                else if (e->op == ">=") result = (int)(lv >= rv);
                else if (e->op == "==") result = (int)(lv == rv);
                else if (e->op == "!=") result = (int)(lv != rv);
                else if (e->op == "&&") result = (int)(lv && rv);
                else if (e->op == "||") result = (int)(lv || rv);
                else if (e->op == "&")  result = lv & rv;
                else if (e->op == "|")  result = lv | rv;
                else if (e->op == "^")  result = lv ^ rv;
                else if (e->op == "<<") result = lv << rv;
                else if (e->op == ">>") result = lv >> rv;
                else valid = false;

                if (valid) return Expr::makeConst(result);
            }

            // Return updated tree even if not fully folded
            if (l.get() != e->left.get() || r.get() != e->right.get())
                return Expr::makeBinOp(e->op, l, r);
            return e;
        }

        if (e->kind == Expr::UNARYOP) {
            auto sub = foldExpr(e->operand);
            if (sub->isConst()) {
                int v = sub->intVal;
                if      (e->op == "-") return Expr::makeConst(-v);
                else if (e->op == "!") return Expr::makeConst((int)(!v));
                else if (e->op == "~") return Expr::makeConst(~v);
            }
            if (sub.get() != e->operand.get())
                return Expr::makeUnaryOp(e->op, sub);
            return e;
        }

        return e;
    }

    // ── Replace variable references with constants ───────────
    bool propagateInExpr(ExprPtr &e, const std::map<std::string, int> &constants) {
        if (!e) return false;
        bool changed = false;

        if (e->kind == Expr::VAR) {
            auto it = constants.find(e->varName);
            if (it != constants.end()) {
                e = Expr::makeConst(it->second);
                return true;
            }
        } else if (e->kind == Expr::BINOP) {
            changed |= propagateInExpr(e->left, constants);
            changed |= propagateInExpr(e->right, constants);
        } else if (e->kind == Expr::UNARYOP) {
            changed |= propagateInExpr(e->operand, constants);
        }

        return changed;
    }

    // ── Remove a single edge ─────────────────────────────────
    void removeEdge(int fromId, int toId) {
        auto &from = cfg.blocks[fromId];
        auto &to   = cfg.blocks[toId];
        from.successors.erase(
            std::remove(from.successors.begin(),
                        from.successors.end(), toId),
            from.successors.end());
        to.predecessors.erase(
            std::remove(to.predecessors.begin(),
                        to.predecessors.end(), fromId),
            to.predecessors.end());
    }
};
