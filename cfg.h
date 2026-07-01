#pragma once
// ============================================================
// cfg.h  —  Control Flow Graph data structures
//
// Mirrors Python's cfg_builder.py:
//   BasicBlock  – node in the CFG (id, instructions, edges)
//   CFG         – collection of blocks plus entry/exit pointers
// ============================================================

#include "ir.h"

#include <map>
#include <vector>
#include <set>
#include <string>
#include <algorithm>
#include <queue>

// ────────────────────────────────────────────────────────────
// BasicBlock
// ────────────────────────────────────────────────────────────

struct BasicBlock {
    int id;
    std::vector<Instruction> instructions;
    std::vector<int> successors;
    std::vector<int> predecessors;

    explicit BasicBlock(int _id = 0) : id(_id) {}

    void addInstr(const Instruction &inst) {
        instructions.push_back(inst);
    }
};

// ────────────────────────────────────────────────────────────
// CFG
// ────────────────────────────────────────────────────────────

class CFG {
public:
    std::string name;
    std::map<int, BasicBlock> blocks;
    int entryId = -1;
    int exitId  = -1;
    int nextId  = 0;

    explicit CFG(const std::string &n = "main") : name(n) {}

    int createBlock() {
        int id = nextId++;
        blocks.emplace(id, BasicBlock(id));
        return id;
    }

    void addEdge(int from, int to) {
        if (from < 0 || to < 0) return;
        auto &src = blocks[from];
        auto &dst = blocks[to];
        if (std::find(src.successors.begin(), src.successors.end(), to)
                == src.successors.end())
            src.successors.push_back(to);
        if (std::find(dst.predecessors.begin(), dst.predecessors.end(), from)
                == dst.predecessors.end())
            dst.predecessors.push_back(from);
    }

    void removeBlock(int id) {
        if (blocks.find(id) == blocks.end()) return;
        auto &bb = blocks[id];
        // Clean up predecessors → successor lists
        for (int predId : bb.predecessors) {
            auto &pred = blocks[predId];
            pred.successors.erase(
                std::remove(pred.successors.begin(), pred.successors.end(), id),
                pred.successors.end());
        }
        // Clean up successors → predecessor lists
        for (int succId : bb.successors) {
            auto &succ = blocks[succId];
            succ.predecessors.erase(
                std::remove(succ.predecessors.begin(), succ.predecessors.end(), id),
                succ.predecessors.end());
        }
        blocks.erase(id);
    }

    // Check if a block is terminated (has successors wired up,
    // indicating control has been directed elsewhere).
    bool isTerminated(int id) const {
        auto it = blocks.find(id);
        if (it == blocks.end()) return true;
        const auto &bb = it->second;
        if (!bb.successors.empty()) {
            // Check if last instruction is return, break, continue, goto
            if (!bb.instructions.empty()) {
                auto &last = bb.instructions.back();
                if (last.kind == Instruction::RETURN_STMT) return true;
            }
            return false;
        }
        return false;
    }

    // Statistics
    struct Stats {
        int blocks = 0, edges = 0, statements = 0;
    };
    Stats stats() const {
        Stats s;
        s.blocks = (int)blocks.size();
        for (auto &[id, bb] : blocks) {
            s.edges += (int)bb.successors.size();
            s.statements += (int)bb.instructions.size();
        }
        return s;
    }
};
