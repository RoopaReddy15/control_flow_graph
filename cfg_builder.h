#pragma once
// ============================================================
// cfg_builder.h  —  Clang AST  →  CFG
//
// Mirrors Python's core/ast_to_cfg.py (AST_to_CFG_Builder).
//
// Uses Clang's AST API (RecursiveASTVisitor for top-level
// function discovery, then manual recursive dispatch for
// full control over every C construct).
//
// Supported constructs (matching the Python version):
//   if/else, while, for, do-while, switch/case/default,
//   break, continue, return, goto, label, declarations,
//   assignments, standalone function calls.
// ============================================================

#include "cfg.h"

#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/Basic/LangOptions.h>
#include <llvm/Support/raw_ostream.h>

#include <stack>
#include <string>
#include <map>

// ── Helper: pretty-print a Clang Expr to string ─────────────
inline std::string clangExprStr(clang::Expr *e) {
    if (!e) return "?";
    std::string buf;
    llvm::raw_string_ostream os(buf);
    e->printPretty(os, nullptr,
                   clang::PrintingPolicy(clang::LangOptions()));
    return os.str();
}

// ────────────────────────────────────────────────────────────
// Convert Clang Expr → our IR Expr
// ────────────────────────────────────────────────────────────

inline ExprPtr convertExpr(clang::Expr *raw) {
    using namespace clang;
    if (!raw) return ::Expr::makeUnknown("?");
    raw = raw->IgnoreImpCasts();
    raw = raw->IgnoreParens();

    // Integer literal
    if (auto *lit = llvm::dyn_cast<IntegerLiteral>(raw))
        return ::Expr::makeConst((int)lit->getValue().getSExtValue());

    // Character literal (e.g. 'a')
    if (auto *ch = llvm::dyn_cast<CharacterLiteral>(raw))
        return ::Expr::makeConst((int)ch->getValue());

    // Variable reference
    if (auto *ref = llvm::dyn_cast<DeclRefExpr>(raw))
        return ::Expr::makeVar(ref->getNameInfo().getAsString());

    // Binary operator
    if (auto *bin = llvm::dyn_cast<BinaryOperator>(raw)) {
        auto l = convertExpr(bin->getLHS());
        auto r = convertExpr(bin->getRHS());
        return ::Expr::makeBinOp(
            BinaryOperator::getOpcodeStr(bin->getOpcode()).str(), l, r);
    }

    // Unary operator
    if (auto *un = llvm::dyn_cast<UnaryOperator>(raw)) {
        auto sub = convertExpr(un->getSubExpr());
        std::string op;
        switch (un->getOpcode()) {
        case UO_Minus:   op = "-";   break;
        case UO_Not:     op = "~";   break;
        case UO_LNot:    op = "!";   break;
        case UO_PreInc:  op = "++";  break;
        case UO_PreDec:  op = "--";  break;
        case UO_PostInc: op = "p++"; break;
        case UO_PostDec: op = "p--"; break;
        case UO_AddrOf:  op = "&";   break;
        case UO_Deref:   op = "*";   break;
        default:         op = "?";   break;
        }
        return ::Expr::makeUnaryOp(op, sub);
    }

    // Function call
    if (auto *call = llvm::dyn_cast<CallExpr>(raw)) {
        std::string fn;
        if (auto *callee = call->getDirectCallee())
            fn = callee->getNameAsString();
        else
            fn = clangExprStr(call->getCallee());

        std::vector<ExprPtr> args;
        for (unsigned i = 0; i < call->getNumArgs(); i++)
            args.push_back(convertExpr(call->getArg(i)));
        return ::Expr::makeCall(fn, std::move(args));
    }

    // String literal
    if (auto *str = llvm::dyn_cast<StringLiteral>(raw))
        return ::Expr::makeUnknown("\"" + str->getString().str() + "\"");

    // Compound assignment (+=, -=, etc.) — treat as binary op
    if (auto *ca = llvm::dyn_cast<CompoundAssignOperator>(raw)) {
        auto l = convertExpr(ca->getLHS());
        auto r = convertExpr(ca->getRHS());
        return ::Expr::makeBinOp(
            BinaryOperator::getOpcodeStr(ca->getOpcode()).str(), l, r);
    }

    // Array subscript  a[i]
    if (auto *arr = llvm::dyn_cast<ArraySubscriptExpr>(raw)) {
        auto base = convertExpr(arr->getBase());
        auto idx  = convertExpr(arr->getIdx());
        // Represent as base[idx]
        return ::Expr::makeUnknown(
            base->toString() + "[" + idx->toString() + "]");
    }

    // Conditional expr  (c ? t : f)
    if (auto *cond = llvm::dyn_cast<ConditionalOperator>(raw)) {
        return ::Expr::makeUnknown(clangExprStr(raw));
    }

    // Fallback
    return ::Expr::makeUnknown(clangExprStr(raw));
}

// ────────────────────────────────────────────────────────────
// CFGBuilder — builds one CFG per function
// ────────────────────────────────────────────────────────────

class CFGBuilder {
public:
    CFG cfg;

    CFGBuilder(const std::string &funcName) : cfg(funcName) {
        // Create ENTRY block
        int entry = cfg.createBlock();
        cfg.entryId = entry;
        cfg.blocks[entry].addInstr(Instruction::makeEntry());

        // Create EXIT block
        int exit = cfg.createBlock();
        cfg.exitId = exit;
        cfg.blocks[exit].addInstr(Instruction::makeExit());

        currentBlock = entry;
    }

    CFG build(clang::Stmt *body) {
        visitStmt(body);

        // Link final block → EXIT if it has no successors
        if (currentBlock >= 0 && cfg.blocks.count(currentBlock)) {
            auto &bb = cfg.blocks[currentBlock];
            if (bb.successors.empty())
                cfg.addEdge(currentBlock, cfg.exitId);
        }

        // Resolve pending gotos
        for (auto &[src, label] : pendingGotos) {
            if (labels.count(label))
                cfg.addEdge(src, labels[label]);
        }

        return std::move(cfg);
    }

private:
    int currentBlock = -1;

    // Break / continue target stacks (mirrors Python)
    std::vector<int> breakTargets;
    std::vector<int> continueTargets;

    // Goto / label support
    std::map<std::string, int> labels;
    std::vector<std::pair<int, std::string>> pendingGotos;

    // ── New block creation ───────────────────────────────────
    int freshBlock() { return cfg.createBlock(); }

    // ── Top-level statement dispatch ─────────────────────────
    void visitStmt(clang::Stmt *s) {
        if (!s) return;
        using namespace clang;

        if (auto *c = llvm::dyn_cast<CompoundStmt>(s))       visitCompound(c);
        else if (auto *i = llvm::dyn_cast<IfStmt>(s))        visitIf(i);
        else if (auto *w = llvm::dyn_cast<WhileStmt>(s))     visitWhile(w);
        else if (auto *f = llvm::dyn_cast<ForStmt>(s))       visitFor(f);
        else if (auto *d = llvm::dyn_cast<DoStmt>(s))        visitDoWhile(d);
        else if (auto *sw = llvm::dyn_cast<SwitchStmt>(s))   visitSwitch(sw);
        else if (llvm::isa<BreakStmt>(s))                    visitBreak();
        else if (llvm::isa<ContinueStmt>(s))                 visitContinue();
        else if (auto *r = llvm::dyn_cast<ReturnStmt>(s))    visitReturn(r);
        else if (auto *g = llvm::dyn_cast<GotoStmt>(s))      visitGoto(g);
        else if (auto *l = llvm::dyn_cast<LabelStmt>(s))     visitLabel(l);
        else if (auto *ds = llvm::dyn_cast<DeclStmt>(s))     visitDecl(ds);
        else if (auto *e = llvm::dyn_cast<clang::Expr>(s))          visitExprStmt(e);
        // NullStmt, etc. — nothing to do
    }

    // ── Compound (block of statements) ───────────────────────
    void visitCompound(clang::CompoundStmt *c) {
        for (auto *child : c->body())
            visitStmt(child);
    }

    // ── If / Else ────────────────────────────────────────────
    void visitIf(clang::IfStmt *s) {
        cfg.blocks[currentBlock].addInstr(
            Instruction::makeCond("IF_COND", convertExpr(s->getCond())));
        int condBlock = currentBlock;

        // True path
        int thenId = freshBlock();
        cfg.addEdge(condBlock, thenId);
        currentBlock = thenId;
        visitStmt(s->getThen());
        int endThen = currentBlock;

        // False path
        int endElse = -1;
        if (s->getElse()) {
            int elseId = freshBlock();
            cfg.addEdge(condBlock, elseId);
            currentBlock = elseId;
            visitStmt(s->getElse());
            endElse = currentBlock;
        }

        // Merge
        int mergeId = freshBlock();
        if (endThen >= 0 && !cfg.isTerminated(endThen))
            cfg.addEdge(endThen, mergeId);
        if (endElse >= 0 && !cfg.isTerminated(endElse))
            cfg.addEdge(endElse, mergeId);
        else if (!s->getElse())
            cfg.addEdge(condBlock, mergeId);

        currentBlock = mergeId;
    }

    // ── While ────────────────────────────────────────────────
    void visitWhile(clang::WhileStmt *s) {
        int headerBlock = freshBlock();
        cfg.addEdge(currentBlock, headerBlock);
        cfg.blocks[headerBlock].addInstr(
            Instruction::makeCond("WHILE_COND", convertExpr(s->getCond())));

        int exitBlock = freshBlock();

        breakTargets.push_back(exitBlock);
        continueTargets.push_back(headerBlock);

        int bodyBlock = freshBlock();
        cfg.addEdge(headerBlock, bodyBlock);
        currentBlock = bodyBlock;
        visitStmt(s->getBody());
        if (!cfg.isTerminated(currentBlock))
            cfg.addEdge(currentBlock, headerBlock); // back edge

        cfg.addEdge(headerBlock, exitBlock);

        breakTargets.pop_back();
        continueTargets.pop_back();
        currentBlock = exitBlock;
    }

    // ── For ──────────────────────────────────────────────────
    void visitFor(clang::ForStmt *s) {
        // Init
        if (s->getInit()) visitStmt(s->getInit());

        // Header
        int headerBlock = freshBlock();
        cfg.addEdge(currentBlock, headerBlock);
        if (s->getCond())
            cfg.blocks[headerBlock].addInstr(
                Instruction::makeCond("FOR_COND", convertExpr(s->getCond())));

        int exitBlock = freshBlock();

        // Increment block (continue target)
        int incrBlock = freshBlock();

        breakTargets.push_back(exitBlock);
        continueTargets.push_back(incrBlock);

        // Body
        int bodyBlock = freshBlock();
        cfg.addEdge(headerBlock, bodyBlock);
        currentBlock = bodyBlock;
        visitStmt(s->getBody());
        if (!cfg.isTerminated(currentBlock))
            cfg.addEdge(currentBlock, incrBlock);

        // Increment
        if (s->getInc())
            cfg.blocks[incrBlock].addInstr(
                Instruction::makeForNext(convertExpr(s->getInc())));
        cfg.addEdge(incrBlock, headerBlock); // back edge

        cfg.addEdge(headerBlock, exitBlock);

        breakTargets.pop_back();
        continueTargets.pop_back();
        currentBlock = exitBlock;
    }

    // ── Do-While ────────────────────────────────────────────
    void visitDoWhile(clang::DoStmt *s) {
        int bodyBlock = freshBlock();
        cfg.addEdge(currentBlock, bodyBlock);

        int exitBlock = freshBlock();
        int condBlock = freshBlock();

        breakTargets.push_back(exitBlock);
        continueTargets.push_back(condBlock);

        currentBlock = bodyBlock;
        visitStmt(s->getBody());
        if (!cfg.isTerminated(currentBlock))
            cfg.addEdge(currentBlock, condBlock);

        cfg.blocks[condBlock].addInstr(
            Instruction::makeCond("DOWHILE_COND", convertExpr(s->getCond())));
        cfg.addEdge(condBlock, bodyBlock);  // true  → loop back
        cfg.addEdge(condBlock, exitBlock);  // false → exit

        breakTargets.pop_back();
        continueTargets.pop_back();
        currentBlock = exitBlock;
    }

    // ── Switch ───────────────────────────────────────────────
    void visitSwitch(clang::SwitchStmt *s) {
        cfg.blocks[currentBlock].addInstr(
            Instruction::makeSwitchExpr(convertExpr(s->getCond())));
        int switchBlock = currentBlock;

        int switchExit = freshBlock();
        breakTargets.push_back(switchExit);

        int prevCaseEnd = -1;

        // The switch body is typically a CompoundStmt
        if (auto *body = llvm::dyn_cast<clang::CompoundStmt>(s->getBody())) {
            for (auto *child : body->body()) {
                if (auto *cs = llvm::dyn_cast<clang::CaseStmt>(child)) {
                    int caseBlock = freshBlock();
                    cfg.addEdge(switchBlock, caseBlock);
                    if (prevCaseEnd >= 0 && !cfg.isTerminated(prevCaseEnd))
                        cfg.addEdge(prevCaseEnd, caseBlock); // fall-through
                    currentBlock = caseBlock;
                    cfg.blocks[caseBlock].addInstr(
                        Instruction::makeCase(convertExpr(cs->getLHS())));
                    visitStmt(cs->getSubStmt());
                    prevCaseEnd = currentBlock;
                } else if (auto *ds = llvm::dyn_cast<clang::DefaultStmt>(child)) {
                    int defBlock = freshBlock();
                    cfg.addEdge(switchBlock, defBlock);
                    if (prevCaseEnd >= 0 && !cfg.isTerminated(prevCaseEnd))
                        cfg.addEdge(prevCaseEnd, defBlock);
                    currentBlock = defBlock;
                    cfg.blocks[defBlock].addInstr(Instruction::makeDefault());
                    visitStmt(ds->getSubStmt());
                    prevCaseEnd = currentBlock;
                } else {
                    // Statement within current case (after the first sub-stmt)
                    visitStmt(child);
                    prevCaseEnd = currentBlock;
                }
            }
        }

        // Link last case to exit if not terminated
        if (prevCaseEnd >= 0 && !cfg.isTerminated(prevCaseEnd))
            cfg.addEdge(prevCaseEnd, switchExit);

        breakTargets.pop_back();
        currentBlock = switchExit;
    }

    // ── Return ───────────────────────────────────────────────
    void visitReturn(clang::ReturnStmt *s) {
        ExprPtr retVal = s->getRetValue() ?
            convertExpr(s->getRetValue()) : nullptr;
        cfg.blocks[currentBlock].addInstr(Instruction::makeReturn(retVal));
        cfg.addEdge(currentBlock, cfg.exitId);
        // Dead block after return
        currentBlock = freshBlock();
    }

    // ── Break ────────────────────────────────────────────────
    void visitBreak() {
        if (!breakTargets.empty())
            cfg.addEdge(currentBlock, breakTargets.back());
        currentBlock = freshBlock(); // dead block
    }

    // ── Continue ─────────────────────────────────────────────
    void visitContinue() {
        if (!continueTargets.empty())
            cfg.addEdge(currentBlock, continueTargets.back());
        currentBlock = freshBlock(); // dead block
    }

    // ── Goto ─────────────────────────────────────────────────
    void visitGoto(clang::GotoStmt *s) {
        pendingGotos.push_back({currentBlock, s->getLabel()->getName().str()});
        currentBlock = freshBlock(); // dead block
    }

    // ── Label ────────────────────────────────────────────────
    void visitLabel(clang::LabelStmt *s) {
        int labelBlock = freshBlock();
        if (!cfg.isTerminated(currentBlock))
            cfg.addEdge(currentBlock, labelBlock);
        currentBlock = labelBlock;
        cfg.blocks[labelBlock].addInstr(
            Instruction::makeLabel(s->getName()));
        labels[s->getName()] = labelBlock;
        visitStmt(s->getSubStmt());
    }

    // ── Declarations ─────────────────────────────────────────
    void visitDecl(clang::DeclStmt *s) {
        using namespace clang;
        for (auto *decl : s->decls()) {
            if (auto *var = llvm::dyn_cast<VarDecl>(decl)) {
                if (var->hasInit()) {
                    cfg.blocks[currentBlock].addInstr(
                        Instruction::makeDeclAssign(
                            var->getNameAsString(),
                            convertExpr(var->getInit())));
                } else {
                    cfg.blocks[currentBlock].addInstr(
                        Instruction::makeDeclOnly(var->getNameAsString()));
                }
            }
        }
    }

    // ── Expression statements ────────────────────────────────
    void visitExprStmt(clang::Expr *e) {
        using namespace clang;
        e = e->IgnoreImpCasts();
        e = e->IgnoreParens();

        // Assignment  (x = expr)
        if (auto *bin = llvm::dyn_cast<BinaryOperator>(e)) {
            if (bin->isAssignmentOp()) {
                if (auto *lhs = llvm::dyn_cast<DeclRefExpr>(bin->getLHS())) {
                    std::string var = lhs->getNameInfo().getAsString();
                    if (bin->isCompoundAssignmentOp()) {
                        // e.g. x += 5  →  x = (x + 5)  (as IR)
                        auto rhsExpr = convertExpr(bin->getRHS());
                        std::string baseOp;
                        switch (bin->getOpcode()) {
                        case BO_AddAssign: baseOp = "+"; break;
                        case BO_SubAssign: baseOp = "-"; break;
                        case BO_MulAssign: baseOp = "*"; break;
                        case BO_DivAssign: baseOp = "/"; break;
                        case BO_RemAssign: baseOp = "%"; break;
                        default: baseOp = "?"; break;
                        }
                        auto full = ::Expr::makeBinOp(baseOp,
                            ::Expr::makeVar(var), rhsExpr);
                        cfg.blocks[currentBlock].addInstr(
                            Instruction::makeAssign(var, full));
                    } else {
                        cfg.blocks[currentBlock].addInstr(
                            Instruction::makeAssign(var,
                                convertExpr(bin->getRHS())));
                    }
                    return;
                }
            }
        }

        // Standalone function call
        if (auto *call = llvm::dyn_cast<CallExpr>(e)) {
            std::string fn;
            if (auto *callee = call->getDirectCallee())
                fn = callee->getNameAsString();
            else
                fn = clangExprStr(call->getCallee());

            std::vector<ExprPtr> args;
            for (unsigned i = 0; i < call->getNumArgs(); i++)
                args.push_back(convertExpr(call->getArg(i)));

            cfg.blocks[currentBlock].addInstr(
                Instruction::makeCallStmt(fn, std::move(args)));
            return;
        }

        // Unary increment/decrement as statement (i++)
        if (auto *un = llvm::dyn_cast<UnaryOperator>(e)) {
            if (un->isIncrementDecrementOp()) {
                auto *sub = un->getSubExpr()->IgnoreImpCasts();
                if (auto *ref = llvm::dyn_cast<DeclRefExpr>(sub)) {
                    std::string var = ref->getNameInfo().getAsString();
                    std::string op = un->isIncrementOp() ? "+" : "-";
                    auto full = ::Expr::makeBinOp(op,
                        ::Expr::makeVar(var), ::Expr::makeConst(1));
                    cfg.blocks[currentBlock].addInstr(
                        Instruction::makeAssign(var, full));
                    return;
                }
            }
        }

        // Fallback: add as unknown
        cfg.blocks[currentBlock].addInstr(
            Instruction::makeAssign("_", convertExpr(e)));
    }
};
