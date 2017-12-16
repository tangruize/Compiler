//
// Created by tangruize on 17-12-14.
//

#include "ir.h"
#include "tree.h"
#include "symbol.h"

#include <list>
#include <string>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cassert>

using namespace std;

enum {IR_NONE, OP_VARIABLE = 1, OP_INTCONST, OP_LABEL};
enum {IC_ASSIGN = 1, IC_ADD, IC_SUB, IC_MUL, IC_DIV,
    IC_DEREF, IC_REF, IC_GOTO, IC_RETURN, IC_DEC, IC_LABEL,
    IC_FUNC, IC_CALL, IC_PARAM, IC_ARG, IC_READ, IC_WRITE,
    IC_RELOP, IC_EQ = IC_RELOP, IC_NE, IC_LT, IC_GT, IC_LE, IC_GE};

static const vector<string> relOp = {"==", "!=", "<", ">", "<=", ">="};
static const vector<string> icString = {
/* IR_NONE */   "",
/* IC_ASSIGN */ "%s := %s",
/* IC_ADD */    "%s := %s + %s",
/* IC_SUB */    "%s := %s - %s",
/* IC_MUL */    "%s := %s * %s",
/* IC_DIV */    "%s := %s / %s",
/* IC_DEREF */  "%s := *%s",
/* IC_REF */    "%s := &%s",
/* IC_GOTO */   "GOTO %s",
/* IC_RETURN */ "RETURN %s",
/* IC_DEC */    "DEC %s %s",
/* IC_LABEL */  "LABEL %s :",
/* IC_FUNC */   "FUNCTION %s :",
/* IC_CALL */   "%s := CALL %s",
/* IC_PARAM */  "PARAM %s",
/* IC_ARG */    "ARG %s",
/* IC_READ */   "READ %s",
/* IC_WRITE */  "WRITE %s",
/* IC_EQ */     "IF %s == %s GOTO %s",
/* IC_NE */     "IF %s != %s GOTO %s",
/* IC_LT */     "IF %s < %s GOTO %s",
/* IC_GT */     "IF %s > %s GOTO %s",
/* IC_LE */     "IF %s <= %s GOTO %s",
/* IC_GE */     "IF %s >= %s GOTO %s"
};

struct Operand {
    int kind;
    string value;
    Operand() {}
    Operand(int k, const string &v): kind(k), value(v) {}
} ;

typedef struct Operand Operand;

struct InterCode {
    int kind;
    Operand target, arg1, arg2;
    InterCode(int k = IR_NONE) {
        kind = k;
        target.kind = arg1.kind = arg2.kind = IR_NONE;
    }
    InterCode(int k, const Operand &t) {
        kind = k;
        target = t;
        arg1.kind = arg2.kind = IR_NONE;
    }
    InterCode(int k, const Operand &t, const Operand &a1) {
        kind = k;
        target = t;
        arg1 = a1;
        arg2.kind = IR_NONE;
    }
    InterCode(int k, const Operand &t, const Operand &a1, const Operand &a2) {
        kind = k;
        target = t;
        arg1 = a1;
        arg2 = a2;
    }
};

typedef struct InterCode InterCode;
typedef list<InterCode> IRList;

class IrSim {
private:
    int tmpSeq = 1;
    int labelSeq = 1;
    const string tmpVarPrefix = "t";
    const string labelPrefix = "label";
    const size_t PRINT_BUF_SIZE = 256;
    char *printBuf;
    IRList irList;
    IRList *curIrList;

public:
    IrSim() { printBuf = new char[PRINT_BUF_SIZE]; }
    ~IrSim() { delete[] printBuf; }
    Operand newTmpVar() { return Operand(OP_VARIABLE, tmpVarPrefix + to_string(tmpSeq++)); }
    Operand newLabel()  { return Operand(OP_LABEL, labelPrefix + to_string(labelSeq++)); }
    IRList *setCurIrList(IRList &cil) { return curIrList = &cil; }
    IrSim &pushIrList() {
        copy(curIrList->begin(), curIrList->end(), back_inserter(irList));
        return *this;
    }
    IrSim &pushIr(const InterCode &ic) {
        curIrList->push_back(ic);
        return *this;
    }

public:
    int st2it(ast *node) const {
        if (!node)         return IR_NONE;
        switch (node->type) {
            case PLUS:     return IC_ADD;
            case MINUS:    return IC_SUB;
            case STAR:     return IC_MUL;
            case DIV:      return IC_DIV;
            case RELOP:    return IC_RELOP + (find(relOp.begin(), relOp.end(), node->lex) - relOp.begin());
            default:       abort();
        }
    }
    int st1it(ast *node) const {
        if (!node)         return IR_NONE;
        switch (node->type) {
            case RETURN:   return IC_RETURN;
            case ASSIGNOP: return IC_ASSIGN;
            case STAR:     return IC_DEREF;
            case ADDRESS:  return IC_REF;
            default:       return st2it(node);
        }
    }

public:
    Operand opValue(ast *node) const {
        assert(node && node->type == INT);
        return Operand(OP_INTCONST, node->lex);
    }
    Operand opVariable(ast *node) const {
        assert(node && node->type == ID);
        return Operand(OP_VARIABLE, node->lex);
    }
    Operand opValue(const string &s) const { return Operand(OP_INTCONST, s); }
    Operand opVariable(const string &s) const { return Operand(OP_VARIABLE, s); }

public:
    InterCode icBinOp(int kind, const Operand &t, const Operand &a1, const Operand &a2) const {
        return InterCode(kind, t, a1, a2);
    }
    InterCode icUnaryOp(int kind, const Operand &t, const Operand &a1) const { return InterCode(kind, t, a1); }
    InterCode icNoOp(int kind, const Operand &t) const { return InterCode(kind, t); }

public:
    InterCode icIfGoto(ast *node, const Operand &t, const Operand &a1, const Operand &a2) const {
        return icBinOp(st2it(node), t, a1, a2);
    }
    InterCode icCall(const Operand &t, const Operand &a1) const { return icUnaryOp(IC_CALL, t, a1); }
    InterCode icDec(const Operand &t, const Operand &a1) const { return icUnaryOp(IC_DEC, t, a1); }
    InterCode icGoto(const Operand &t)     const { return icNoOp(IC_GOTO, t); }
    InterCode icReturn(const Operand &t)   const { return icNoOp(IC_RETURN, t); }
    InterCode icFunction(const Operand &t) const { return icNoOp(IC_FUNC, t); }
    InterCode icLabel(const Operand &t)    const { return icNoOp(IC_LABEL, t); }
    InterCode icArg(const Operand &t)      const { return icNoOp(IC_ARG, t); }
    InterCode icParam(const Operand &t)    const { return icNoOp(IC_PARAM, t); }
    InterCode icRead(const Operand &t)     const { return icNoOp(IC_READ, t); }
    InterCode icWrite(const Operand &t)    const { return icNoOp(IC_WRITE, t); }

public:
    const char *print3(const InterCode &i) const {
        if (i.arg2.kind == IR_NONE) return print2(i);
        if (i.kind < IC_RELOP)
            snprintf(printBuf, PRINT_BUF_SIZE,
                     icString[i.kind].c_str(),
                     i.target.value.c_str(),
                     i.arg1.value.c_str(),
                     i.arg2.value.c_str());
        else
            snprintf(printBuf, PRINT_BUF_SIZE,
                     icString[i.kind].c_str(),
                     i.arg1.value.c_str(),
                     i.arg2.value.c_str(),
                     i.target.value.c_str());
        return printBuf;
    }
    const char *print2(const InterCode &i) const {
        if (i.arg1.kind == IR_NONE) return print1(i);
        snprintf(printBuf, PRINT_BUF_SIZE, icString[i.kind].c_str(), i.target.value.c_str(), i.arg1.value.c_str());
        return printBuf;
    }
    const char *print1(const InterCode &i) const {
        if (i.kind == IR_NONE || i.target.kind == IR_NONE) return "";
        snprintf(printBuf, PRINT_BUF_SIZE, icString[i.kind].c_str(), i.target.value.c_str());
        return printBuf;
    }
    const char *print(const InterCode &i) const { return print3(i); }
    void printStream(ostream &out) const {
        for_each(irList.begin(), irList.end(), [&out, this](const InterCode &i){ out << print(i) << "\n"; });
    }
};

static IrSim ic;

//translate section:

static void TR_FunDec(ast *node) {
    attr *a = findfunc(child(node, 1)->val->idval);
    assert(a);
    IRList funcDec;
    ic.setCurIrList(funcDec);
    ic.pushIr(ic.icFunction(ic.opVariable(child(node, 1))));
    arg_t *args = a->function->arg_list;
    while (args) {
        ic.pushIr(ic.icParam(ic.opVariable(args->arg->basic->name)));
        args = args->next;
    }
    ic.pushIrList();
}

static void TR_CompSt(ast *node) {

}

static void TR_ExtDef(ast *node) {
    struct ast *child2 = child(node, 2);
    if (child2->type == FunDec) {
        TR_FunDec(child2);
        TR_CompSt(sibling(child2, 1));
    }
}

static void TR_ExtDefList(ast *node) {
    if (node->type == NullNode)
        return;
    TR_ExtDef(child(node, 1));
    TR_ExtDefList(child(node, 2));
}

static void TR_Program(ast *node) {
    TR_ExtDefList(child(node, 1));
    ic.printStream(cout);
}

extern "C" void genIR() {
    TR_Program(ast_root);
}

