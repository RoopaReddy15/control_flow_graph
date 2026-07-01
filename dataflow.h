#pragma once

#include "cfg.h"

#include <set>
#include <map>
#include <tuple>
#include <vector>
#include <algorithm>

// A definition is identified by (variable, block_id, stmt_index).
using Definition = std::tuple<std::string, int, int>;

// ────────────────────────────────────────────────────────────
// DataFlowAnalysis
// ────────────────────────────────────────────────────────────

class DataFlowAnalysis {
public:
    explicit DataFlowAnalysis(const CFG &cfg) : cfg(cfg) {}

    struct ReachingResult {
        std::map<int, std::set<Definition>> IN;
        std::map<int, std::set<Definition>> OUT;
    };

    ReachingResult reachingDefinitions() const {
        // Compute GEN sets: definitions generated in each block
        std::map<int, std::set<Definition>> gen;
        std::set<Definition> allDefs;

        for (auto &[bbId, bb] : cfg.blocks) {
            std::set<Definition> bbGen;
            for (int idx = 0; idx < (int)bb.instructions.size(); idx++) {
                auto defs = bb.instructions[idx].getDefs();
                for (auto &d : defs) {
                    Definition def{d, bbId, idx};
                    bbGen.insert(def);
                    allDefs.insert(def);
                }
            }
            gen[bbId] = bbGen;
        }

        // Compute KILL sets: a block kills defs from OTHER blocks
        // that define the same variable.
        std::map<int, std::set<Definition>> kill;
        for (auto &[bbId, bb] : cfg.blocks) {
            std::set<Definition> bbKill;
            for (auto &[var, bId, idx] : allDefs) {
                if (bId != bbId) {
                    // Check if this block defines the same variable
                    bool blockDefinesVar = false;
                    for (auto &[gv, gb, gi] : gen[bbId]) {
                        if (gv == var) { blockDefinesVar = true; break; }
                    }
                    if (blockDefinesVar)
                        bbKill.insert({var, bId, idx});
                }
            }
            kill[bbId] = bbKill;
        }

        // Iterative fixed-point
        ReachingResult result;
        for (auto &[bbId, _] : cfg.blocks) {
            result.IN[bbId]  = {};
            result.OUT[bbId] = gen[bbId];
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (auto &[bbId, bb] : cfg.blocks) {
                auto oldOut = result.OUT[bbId];

                // IN[B] = ∪ OUT[P] for P in predecessors
                std::set<Definition> newIn;
                for (int pred : bb.predecessors)
                    newIn.insert(result.OUT[pred].begin(),
                                 result.OUT[pred].end());
                result.IN[bbId] = newIn;

                // OUT[B] = GEN[B] ∪ (IN[B] − KILL[B])
                std::set<Definition> diff;
                std::set_difference(newIn.begin(), newIn.end(),
                                    kill[bbId].begin(), kill[bbId].end(),
                                    std::inserter(diff, diff.begin()));
                result.OUT[bbId] = gen[bbId];
                result.OUT[bbId].insert(diff.begin(), diff.end());

                if (result.OUT[bbId] != oldOut)
                    changed = true;
            }
        }
        return result;
    }

    // ── Live Variable Analysis ───────────────────────────────
    // Determines if a variable's current value will be used in the future.
    // Direction: backward.  Meet: union.
    // Transfer: IN[B] = USE[B] ∪ (OUT[B] − DEF[B])
    struct LiveResult {
        std::map<int, std::set<std::string>> IN;
        std::map<int, std::set<std::string>> OUT;
    };

    LiveResult liveVariables() const {
        // Compute USE and DEF for each block
        std::map<int, std::set<std::string>> USE, DEF;
//bbid id of that block and bb the block in cfg
//inst is the instruction in that block
        for (auto &[bbId, bb] : cfg.blocks) {
            std::set<std::string> useSet, defSet;
//useset of variables used before any definition in the block
//defset of variables defined before any use in the block
            for (auto &inst : bb.instructions) {
                auto uses = inst.getUses();
                auto defs = inst.getDefs();
                // USE[B]: used before any def in B
                for (auto &u : uses)
                    if (defSet.find(u) == defSet.end())
                        useSet.insert(u);
                // DEF[B]: defined before any use in B
                for (auto &d : defs)
                    if (useSet.find(d) == useSet.end())
                        defSet.insert(d);
            }
            USE[bbId] = useSet;
            DEF[bbId] = defSet;
        }

        // Iterative fixed-point (backward)
        LiveResult result;
        for (auto &[bbId, _] : cfg.blocks) {
            result.IN[bbId]  = {};
            result.OUT[bbId] = {};
        }

        bool changed = true;
        while (changed) {
            changed = false;
            // Iterate in reverse order
            for (auto it = cfg.blocks.rbegin(); it != cfg.blocks.rend(); ++it) {
                int bbId = it->first;
                const auto &bb = it->second;
                auto oldIn = result.IN[bbId];

                // OUT[B] = ∪ IN[S] for S in successors
                std::set<std::string> newOut;
                for (int succ : bb.successors)
                    newOut.insert(result.IN[succ].begin(),
                                 result.IN[succ].end());
                result.OUT[bbId] = newOut;

                // IN[B] = USE[B] ∪ (OUT[B] − DEF[B])
                std::set<std::string> diff;
                std::set_difference(newOut.begin(), newOut.end(),
                                    DEF[bbId].begin(), DEF[bbId].end(),
                                    std::inserter(diff, diff.begin()));
                result.IN[bbId] = USE[bbId];
                result.IN[bbId].insert(diff.begin(), diff.end());

                if (result.IN[bbId] != oldIn)
                    changed = true;
            }
        }
        return result;
    }

    // ── Uninitialized Variable Detection ────────────────────
    // Use-case from pdf_content.txt: "If a point in the CFG
    // uses x, but no definition of x reaches it, you've found
    // a potential bug!"
    struct UninitWarning {
        int blockId;
        int stmtIndex;
        std::string varName;
        std::string context;     // instruction text for context
    };

    std::vector<UninitWarning> detectUninitializedVars() const {
        auto rdResult = reachingDefinitions();
        std::vector<UninitWarning> warnings;

        for (auto &[bbId, bb] : cfg.blocks) {
            // Collect definitions that reach the start of this block
            auto reachingIn = rdResult.IN.count(bbId)
                ? rdResult.IN.at(bbId)
                : std::set<Definition>{};

            // Track definitions generated so far within this block
            std::set<std::string> definedSoFar;
            for (auto &[var, blk, idx] : reachingIn)
                definedSoFar.insert(var);

            for (int i = 0; i < (int)bb.instructions.size(); i++) {
                auto &inst = bb.instructions[i];

                // Check uses: is every used variable defined?
                auto uses = inst.getUses();
                for (auto &u : uses) {
                    if (definedSoFar.find(u) == definedSoFar.end()) {
                        warnings.push_back({
                            bbId, i, u, inst.toString()
                        });
                    }
                }

                // Track definitions from this instruction
                auto defs = inst.getDefs();
                for (auto &d : defs)
                    definedSoFar.insert(d);
            }
        }
        return warnings;
    }

private:
    const CFG &cfg;
};
