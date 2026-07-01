#pragma once
// ============================================================
// dot_export.h  —  Graphviz DOT Export with Styling
//
// Mirrors Python's cfg_builder.py → to_dot().
// Features:
//   • Green-filled entry block, red-filled exit block
//   • True/False edge labels for branch blocks
//   • Rounded boxes with Courier font
//   • Proper escaping of special characters
// ============================================================

#include "cfg.h"

#include <fstream>
#include <sstream>
#include <string>

class DOTExporter {
public:
    static std::string escape(const std::string &s) {
        std::string r;
        for (char c : s) {
            if (c == '"')       r += "\\\"";
            else if (c == '\\') r += "\\\\";
            else if (c == '\n') r += "\\n";
            else                r += c;
        }
        return r;
    }

    static std::string toDOT(const CFG &cfg) {
        std::ostringstream out;
        out << "digraph \"" << cfg.name << "\" {\n";
        out << "  node [shape=box, style=\"filled,rounded\", "
               "fontname=\"Courier New\", fontsize=10];\n";
        out << "  edge [fontname=\"Courier New\", fontsize=9];\n\n";

        for (auto &[bbId, bb] : cfg.blocks) {
            // Build label
            std::string label = "Block " + std::to_string(bbId) + "\\n";
            for (auto &inst : bb.instructions)
                label += escape(inst.toString()) + "\\n";

            // Pick colour
            std::string style;
            if (bbId == cfg.entryId)
                style = ", fillcolor=\"#d4edda\", color=\"#28a745\"";
            else if (bbId == cfg.exitId)
                style = ", fillcolor=\"#f8d7da\", color=\"#dc3545\"";
            else
                style = ", fillcolor=\"#e2e3e5\", color=\"#6c757d\"";

            out << "  " << bbId << " [label=\"" << label << "\"" << style << "];\n";

            // Determine if this block is a branch (condition with 2 successors)
            bool isBranch = false;
            if (!bb.instructions.empty()) {
                for (auto &inst : bb.instructions) {
                    if (inst.kind == Instruction::COND &&
                        bb.successors.size() == 2) {
                        isBranch = true;
                        break;
                    }
                }
            }

            for (size_t i = 0; i < bb.successors.size(); i++) {
                int succ = bb.successors[i];
                if (isBranch && bb.successors.size() == 2) {
                    std::string edgeLabel = (i == 0) ? "True" : "False";
                    std::string color = (i == 0) ? "#28a745" : "#dc3545";
                    out << "  " << bbId << " -> " << succ
                        << " [label=\"" << edgeLabel << "\", color=\""
                        << color << "\", fontcolor=\"" << color << "\"];\n";
                } else {
                    out << "  " << bbId << " -> " << succ << ";\n";
                }
            }
        }

        out << "}\n";
        return out.str();
    }

    static void exportToFile(const CFG &cfg, const std::string &filename) {
        std::ofstream f(filename);
        f << toDOT(cfg);
    }
};
