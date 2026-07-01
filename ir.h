#pragma once

#include <string>
#include <vector>
#include <set>
#include <memory>
#include <sstream>

struct Expr {
    enum Kind { CONST_INT, VAR, BINOP, UNARYOP, FUNC_CALL, UNKNOWN };

    Kind kind = UNKNOWN;

    int intVal = 0;                         
    std::string varName;                 
    std::string op;                         
    std::shared_ptr<Expr> left, right;     
    std::shared_ptr<Expr> operand;      
    std::string funcName;      
    std::vector<std::shared_ptr<Expr>> args; 
    std::string raw;                       

    static std::shared_ptr<Expr> makeConst(int v) {
        auto e = std::make_shared<Expr>();
        e->kind = CONST_INT; e->intVal = v;
        return e;
    }
    static std::shared_ptr<Expr> makeVar(const std::string &n) {
        auto e = std::make_shared<Expr>();
        e->kind = VAR; e->varName = n;
        return e;
    }
    static std::shared_ptr<Expr> makeBinOp(const std::string &o,
                                           std::shared_ptr<Expr> l,
                                           std::shared_ptr<Expr> r) {
        auto e = std::make_shared<Expr>();
        e->kind = BINOP; e->op = o; e->left = l; e->right = r;
        return e;
    }
    static std::shared_ptr<Expr> makeUnaryOp(const std::string &o,
                                             std::shared_ptr<Expr> sub) {
        auto e = std::make_shared<Expr>();
        e->kind = UNARYOP; e->op = o; e->operand = sub;
        return e;
    }
    static std::shared_ptr<Expr> makeCall(const std::string &fn,
                                          std::vector<std::shared_ptr<Expr>> a) {
        auto e = std::make_shared<Expr>();
        e->kind = FUNC_CALL; e->funcName = fn; e->args = std::move(a);
        return e;
    }
    static std::shared_ptr<Expr> makeUnknown(const std::string &s) {
        auto e = std::make_shared<Expr>();
        e->kind = UNKNOWN; e->raw = s;
        return e;
    }

    bool isConst()  const { return kind == CONST_INT; }
    bool isVar()    const { return kind == VAR; }
    bool isCall()   const { return kind == FUNC_CALL; }

    void getUsedVars(std::set<std::string> &vars) const {
        switch (kind) {
        case VAR:       vars.insert(varName); break;
        case BINOP:     if (left) left->getUsedVars(vars);
                        if (right) right->getUsedVars(vars); break;
        case UNARYOP:   if (operand) operand->getUsedVars(vars); break;
        case FUNC_CALL: for (auto &a : args) if (a) a->getUsedVars(vars); break;
        default: break;
        }
    }

    std::string toString() const {
        switch (kind) {
        case CONST_INT: return std::to_string(intVal);
        case VAR:       return varName;
        case BINOP: {
            std::string l = left  ? left->toString()  : "?";
            std::string r = right ? right->toString() : "?";
            return "(" + l + " " + op + " " + r + ")";
        }
        case UNARYOP: {
            std::string o = operand ? operand->toString() : "?";
            if (op == "p++" || op == "p--") return o + op.substr(1);
            return op + o;
        }
        case FUNC_CALL: {
            std::string s = funcName + "(";
            for (size_t i = 0; i < args.size(); i++) {
                if (i) s += ", ";
                s += args[i] ? args[i]->toString() : "?";
            }
            return s + ")";
        }
        case UNKNOWN: return raw;
        }
        return "???";
    }

    std::shared_ptr<Expr> clone() const {
        auto e = std::make_shared<Expr>();
        e->kind = kind; e->intVal = intVal; e->varName = varName;
        e->op = op; e->funcName = funcName; e->raw = raw;
        if (left)    e->left    = left->clone();
        if (right)   e->right   = right->clone();
        if (operand) e->operand = operand->clone();
        for (auto &a : args) e->args.push_back(a ? a->clone() : nullptr);
        return e;
    }
};

using ExprPtr = std::shared_ptr<Expr>;



struct Instruction {
    enum Kind {
        ASSIGN,          
        DECL_ASSIGN,   
        DECL_ONLY,       
        COND,            
        FUNC_CALL_STMT, 
        RETURN_STMT,     
        ENTRY_MARKER,
        EXIT_MARKER,
        SWITCH_EXPR,     
        CASE_LABEL,      
        DEFAULT_LABEL, 
        LABEL_MARKER,    
        BRANCH_PRUNED,  
        FOR_NEXT,        
    };

    Kind kind = ENTRY_MARKER;
    std::string condTag;             
    std::string lhs;                 
    ExprPtr rhs;                     
    ExprPtr cond;                     
    ExprPtr retExpr;                  
    std::string funcName;             
    std::vector<ExprPtr> callArgs;   
    std::string labelName;            
    ExprPtr caseExpr;              
    ExprPtr switchExpr;             
    ExprPtr nextExpr;               

   
    std::set<std::string> getDefs() const {
        std::set<std::string> d;
        if (kind == ASSIGN || kind == DECL_ASSIGN) {
            if (!lhs.empty()) d.insert(lhs);
        }
        return d;
    }

    std::set<std::string> getUses() const {
        std::set<std::string> u;
        switch (kind) {
        case ASSIGN:
        case DECL_ASSIGN:  if (rhs)  rhs->getUsedVars(u); break;
        case COND:         if (cond) cond->getUsedVars(u); break;
        case RETURN_STMT:  if (retExpr) retExpr->getUsedVars(u); break;
        case FUNC_CALL_STMT:
            for (auto &a : callArgs) if (a) a->getUsedVars(u); break;
        case SWITCH_EXPR:  if (switchExpr) switchExpr->getUsedVars(u); break;
        case FOR_NEXT:     if (nextExpr) nextExpr->getUsedVars(u); break;
        default: break;
        }
        return u;
    }

    std::string toString() const {
        switch (kind) {
        case ASSIGN:       return lhs + " = " + (rhs ? rhs->toString() : "?");
        case DECL_ASSIGN:  return lhs + " = " + (rhs ? rhs->toString() : "?");
        case DECL_ONLY:    return "decl " + lhs;
        case COND:         return condTag + ": " + (cond ? cond->toString() : "?");
        case FUNC_CALL_STMT: {
            std::string s = funcName + "(";
            for (size_t i = 0; i < callArgs.size(); i++) {
                if (i) s += ", ";
                s += callArgs[i] ? callArgs[i]->toString() : "?";
            }
            return s + ")";
        }
        case RETURN_STMT:  return retExpr ? "return " + retExpr->toString() : "return";
        case ENTRY_MARKER: return "ENTRY";
        case EXIT_MARKER:  return "EXIT";
        case SWITCH_EXPR:  return "switch(" + (switchExpr ? switchExpr->toString() : "?") + ")";
        case CASE_LABEL:   return "case " + (caseExpr ? caseExpr->toString() : "?") + ":";
        case DEFAULT_LABEL: return "default:";
        case LABEL_MARKER: return labelName + ":";
        case BRANCH_PRUNED: return "[branch pruned]";
        case FOR_NEXT:     return "for_next: " + (nextExpr ? nextExpr->toString() : "?");
        }
        return "???";
    }

    static Instruction makeAssign(const std::string &v, ExprPtr e) {
        Instruction i; i.kind = ASSIGN; i.lhs = v; i.rhs = e; return i;
    }
    static Instruction makeDeclAssign(const std::string &v, ExprPtr e) {
        Instruction i; i.kind = DECL_ASSIGN; i.lhs = v; i.rhs = e; return i;
    }
    static Instruction makeDeclOnly(const std::string &v) {
        Instruction i; i.kind = DECL_ONLY; i.lhs = v; return i;
    }
    static Instruction makeCond(const std::string &tag, ExprPtr c) {
        Instruction i; i.kind = COND; i.condTag = tag; i.cond = c; return i;
    }
    static Instruction makeCallStmt(const std::string &fn, std::vector<ExprPtr> a) {
        Instruction i; i.kind = FUNC_CALL_STMT; i.funcName = fn;
        i.callArgs = std::move(a); return i;
    }
    static Instruction makeReturn(ExprPtr e = nullptr) {
        Instruction i; i.kind = RETURN_STMT; i.retExpr = e; return i;
    }
    static Instruction makeEntry() { Instruction i; i.kind = ENTRY_MARKER; return i; }
    static Instruction makeExit()  { Instruction i; i.kind = EXIT_MARKER;  return i; }
    static Instruction makeSwitchExpr(ExprPtr e) {
        Instruction i; i.kind = SWITCH_EXPR; i.switchExpr = e; return i;
    }
    static Instruction makeCase(ExprPtr e) {
        Instruction i; i.kind = CASE_LABEL; i.caseExpr = e; return i;
    }
    static Instruction makeDefault() { Instruction i; i.kind = DEFAULT_LABEL; return i; }
    static Instruction makeLabel(const std::string &n) {
        Instruction i; i.kind = LABEL_MARKER; i.labelName = n; return i;
    }
    static Instruction makeBranchPruned() { Instruction i; i.kind = BRANCH_PRUNED; return i; }
    static Instruction makeForNext(ExprPtr e) {
        Instruction i; i.kind = FOR_NEXT; i.nextExpr = e; return i;
    }
};
