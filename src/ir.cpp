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
#include <cassert>
#include <cstring>
#include <fstream>

using namespace std;

const char *output_file;

enum {IR_NONE = 0, OP_VARIABLE = 1, OP_INTCONST, OP_LABEL};
enum {IC_ADD = 1, IC_SUB, IC_MUL, IC_DIV, IC_ASSIGN, IC_DEREF, IC_DEREF_L, IC_REF,
    IC_GOTO, IC_RETURN, IC_DEC, IC_LABEL, IC_FUNC, IC_CALL, IC_PARAM, IC_ARG, IC_READ, IC_WRITE,
    IC_RELOP, IC_EQ = IC_RELOP, IC_NE, IC_LT, IC_GT, IC_LE, IC_GE, IC_AND, IC_OR, IC_RELOP_END};

static const vector<string> relOp = {"==", "!=", "<", ">", "<=", ">="};
static const char *icFmt[] = {
/* IR_NONE */   "",
/* IC_ADD */    "%s := %s + %s\n",
/* IC_SUB */    "%s := %s - %s\n",
/* IC_MUL */    "%s := %s * %s\n",
/* IC_DIV */    "%s := %s / %s\n",
/* IC_ASSIGN */ "%s := %s\n",
/* IC_DEREF */  "%s := *%s\n",
/* IC_DEREF_L*/ "*%s := %s\n",
/* IC_REF */    "%s := &%s\n",
/* IC_GOTO */   "GOTO %s\n",
/* IC_RETURN */ "RETURN %s\n",
/* IC_DEC */    "DEC %s %s\n",
/* IC_LABEL */  "LABEL %s :\n",
/* IC_FUNC */   "FUNCTION %s :\n",
/* IC_CALL */   "%s := CALL %s\n",
/* IC_PARAM */  "PARAM %s\n",
/* IC_ARG */    "ARG %s\n",
/* IC_READ */   "READ %s\n",
/* IC_WRITE */  "WRITE %s\n",
/* IC_EQ */     "IF %s == %s GOTO %s\n",
/* IC_NE */     "IF %s != %s GOTO %s\n",
/* IC_LT */     "IF %s < %s GOTO %s\n",
/* IC_GT */     "IF %s > %s GOTO %s\n",
/* IC_LE */     "IF %s <= %s GOTO %s\n",
/* IC_GE */     "IF %s >= %s GOTO %s\n"
};

struct Operand {
    int kind;
    string value;
    Operand(): kind(IR_NONE) {}
    Operand(const string &v): kind(OP_VARIABLE), value(v) {}
    Operand(const char *v):   kind(OP_VARIABLE), value(v) {}
    Operand(int k, const string &v): kind(k), value(v) {}
    string str() const { return kind != OP_INTCONST ? value : "#" + value; }
    bool getInt(int &i) const { if (kind == OP_INTCONST) i = atoi(value.c_str()); return kind == OP_INTCONST; }
};

typedef const Operand C_OP;
C_OP OP_ZERO = Operand(OP_INTCONST, "0");
C_OP OP_ONE = Operand(OP_INTCONST, "1");
C_OP OP_NONE;

struct InterCode {
    int kind;
    Operand target, arg1, arg2;
    InterCode(int k = IR_NONE): kind(k) {}
    InterCode(int k, C_OP &t): kind(k), target(t) {}
    InterCode(int k, C_OP &t, C_OP &a1): kind(k), target(t), arg1(a1) {}
    InterCode(int k, C_OP &t, C_OP &a1, C_OP &a2):
            kind(k), target(t), arg1(a1), arg2(a2) {}
    C_OP &at(size_t i) const {
        switch(i) {
            case 1:  return arg1;
            case 2:  return arg2;
            default: return target;
        }
    }
    C_OP &operator[](size_t i) const { return at(i); }
    Operand &operator[](size_t i) { return const_cast<Operand &>(at(i)); }
    bool doArith(Operand &op) {
        if (kind >= IC_ADD && kind <= IC_DIV) {
            int a1, a2;
            if (arg1.getInt(a1)) {
                if (arg2.getInt(a2))
                    switch (kind) {
                        case IC_ADD: a1 += a2; break;
                        case IC_SUB: a1 -= a2; break;
                        case IC_MUL: a1 *= a2; break;
                        default:     a1 /= a2;
                    }
                else if (kind == IC_SUB)
                    a1 = -a1;
                else
                    return false;
                arg2.kind = IR_NONE;
                arg1 = Operand(OP_INTCONST, to_string(a1));
                kind = IC_ASSIGN;
                return true;
            }
        }
        return false;
    }
};

class IrSim {
private:
    const string tmpVarPrefix = "t";
    const string VarPrefix    = "v";
    const string labelPrefix  = "label";
    const size_t PBSIZE = 256;
    char *pb;
    int tmpSeq;
    int varSeq;
    int labelSeq;
    list<InterCode> irList;

public:
    IrSim(): tmpSeq(1), varSeq(1), labelSeq(1) { pb = new char[PBSIZE]; }
    ~IrSim() { delete[] pb; }
    Operand newTmpVar() { return Operand(OP_VARIABLE, tmpVarPrefix + to_string(tmpSeq++)); }
    Operand newLabel()  { return Operand(OP_LABEL,    labelPrefix  + to_string(labelSeq++)); }
    Operand newVar()    { return Operand(OP_VARIABLE, VarPrefix    + to_string(varSeq++)); }
    IrSim &commitIc(const InterCode &ic) { if (ic.kind && ic[0].kind) irList.push_back(ic); return *this; }
    IrSim &operator<<(const InterCode &ic) {
        return commitIc(ic);
    }

    int ast2ic(ast *node) const {
        if (!node)         return IR_NONE;
        switch (node->type) {
            case RETURN:   return IC_RETURN;
            case ASSIGNOP: return IC_ASSIGN;
            case PLUS:     return IC_ADD;
            case MINUS:    return IC_SUB;
            case STAR:     return IC_MUL;
            case DIV:      return IC_DIV;
            case AND:      return IC_AND;
            case OR:       return IC_OR;
            case RELOP:    return IC_RELOP + (find(relOp.begin(), relOp.end(), node->lex) - relOp.begin());
            default:       abort();
        }
    }

    InterCode icBinOp(int kind, C_OP &t, C_OP &a1, C_OP &a2) const { return InterCode(kind, t, a1, a2); }
    InterCode icUnaryOp(int kind, C_OP &t, C_OP &a1) const { return InterCode(kind, t, a1); }
    InterCode icNoOp(int kind, C_OP &t) const { return InterCode(kind, t); }

    InterCode icLabel(C_OP &t)    const { return icNoOp(IC_LABEL, t); }
    InterCode icFunction(C_OP &t) const { return icNoOp(IC_FUNC, t); }
    InterCode icReturn(C_OP &t)   const { return icNoOp(IC_RETURN, t); }
    InterCode icArg(C_OP &t)      const { return icNoOp(IC_ARG, t); }
    InterCode icParam(C_OP &t)    const { return icNoOp(IC_PARAM, t); }
    InterCode icRead(C_OP &t)     const { return icNoOp(IC_READ, t); }
    InterCode icWrite(C_OP &t)    const { return icNoOp(IC_WRITE, t); }
    InterCode icGoto(C_OP &t)     const { return icNoOp(IC_GOTO, t); }
    
    InterCode icDec(C_OP &t, C_OP &a1)    const { return icUnaryOp(IC_DEC, t, a1); }
    InterCode icCall(C_OP &t, C_OP &a1)   const { return icUnaryOp(IC_CALL, t, a1); }
    InterCode icAssign(C_OP &t, C_OP &a1) const { return icUnaryOp(IC_ASSIGN, t, a1); }
    InterCode icDeref(C_OP &t, C_OP &a1)  const { return icUnaryOp(IC_DEREF, t, a1); }
    InterCode icDerefL(C_OP &t, C_OP &a1) const { return icUnaryOp(IC_DEREF_L, t, a1); }
    InterCode icRef(C_OP &t, C_OP &a1)    const { return icUnaryOp(IC_REF, t, a1); }
    
    InterCode icArith(ast *node, C_OP &t, C_OP &a1, C_OP &a2) const {
        return icBinOp(ast2ic(node), t, a1, a2);
    }
    InterCode icIfGoto(int kind, C_OP &t, C_OP &a1, C_OP &a2) const {
        return icBinOp(kind, t, a1, a2);
    }
    InterCode icIfGoto(ast *node, C_OP &t, C_OP &a1, C_OP &a2) const {
        return icIfGoto(ast2ic(node), t, a1, a2);
    }

    const char *print3(const InterCode &i) const {
        if (i.arg2.kind == IR_NONE) return print2(i);
        if (i.kind < IC_RELOP)
            snprintf(pb, PBSIZE, icFmt[i.kind], i[0].str().c_str(), i[1].str().c_str(), i[2].str().c_str());
        else
            snprintf(pb, PBSIZE, icFmt[i.kind], i[1].str().c_str(), i[2].str().c_str(), i[0].str().c_str());
        return pb;
    }
    const char *print2(const InterCode &i) const {
        if (i.arg1.kind == IR_NONE) return print1(i);
        if (i.kind == IC_SUB)
            snprintf(pb, PBSIZE, icFmt[i.kind], i[0].str().c_str(), OP_ZERO.str().c_str(), i[1].str().c_str());
        else
            snprintf(pb, PBSIZE, icFmt[i.kind], i[0].str().c_str(), i[1].str().c_str());
        return pb;
    }
    const char *print1(const InterCode &i) const {
        snprintf(pb, PBSIZE, icFmt[i.kind], i[0].str().c_str());
        return pb;
    }
    const char *print(const InterCode &i) const { return print3(i); }
    void printStream(ostream &out) const {
        for_each(irList.begin(), irList.end(), [&out, this](const InterCode &i){ out << print(i); });
    }
};

static IrSim irSim;

static void TR_VarDec(ast *node);
static void TR_CompSt(ast *node);

extern "C" void genIR() {
#if COMPILER_VERSION >= 3
    ast *node = child(ast_root, 1);
    while (node && node->type != NullNode) {
        ast *ext = child(node, 1), *func = child(ext, 2), *id = child(func, 1), *var = child(func, 3);
        irSim << irSim.icFunction(id->lex);
        while (var && var->type == VarList) {
            TR_VarDec(child(child(var, 1), 2));
            var = child(var, 3);
        }
        TR_CompSt(child(ext, 3));
        node = child(node, 2);
    }
    irSim.printStream(cout);
    ofstream out(output_file);
    if (out.is_open()) {
        irSim.printStream(out);
        out.close();
    }
#endif
}

//translate section:
static void TR_Exp(ast *node, C_OP &place);
static void TR_Cond(ast *node, C_OP &lt, C_OP &lf);
static void TR_Stmt(ast *node);
static Operand TR_Args(ast *node, bool isWrite = false);

//////////////////////////////////////////// EXPR
static void TR_ID(ast *node, C_OP &place) {
    attr *a = findvar(node->lex);
    assert(a);
    irSim << irSim.icAssign(place, a->basic->name);
}

static void TR_INT(ast *node, C_OP &place) {
    irSim << irSim.icAssign(place, Operand(OP_INTCONST, node->lex));
}

static void TR_ASSIGNOP(ast *node, C_OP &place) {
    ast *pre_sibling1 = sibling(node, -1), *exp1_id_node;
    assert(pre_sibling1);
    exp1_id_node = child(pre_sibling1, 1);
    assert(exp1_id_node->type == ID);
    attr *exp1_id = findvar(exp1_id_node->lex);
    assert(exp1_id);
    Operand t1 = irSim.newTmpVar();
    TR_Exp(sibling(node, 1), t1);
    irSim << irSim.icAssign(exp1_id->basic->name, t1) << irSim.icAssign(place, exp1_id->basic->name);
}

static void TR_ARITH(ast *node, C_OP &place) {
    int type = irSim.ast2ic(node);
    if (type >= IC_RELOP && type < IC_RELOP_END) {
        Operand label1 = irSim.newLabel(), label2 = irSim.newLabel();
        irSim << irSim.icAssign(place, OP_ZERO);
        TR_Cond(node, label1, label2);
        irSim << irSim.icLabel(label1) << irSim.icAssign(place, OP_ONE) << irSim.icLabel(label2);
        return;
    }
    Operand t1 = irSim.newTmpVar(), t2 = irSim.newTmpVar();
    TR_Exp(sibling(node, -1), t1);
    TR_Exp(sibling(node, 1), t2);
    irSim << irSim.icArith(node, place, t1, t2);
}

static void TR_MINUS(ast *node, C_OP &place) {
    Operand t1 = irSim.newTmpVar();
    TR_Exp(sibling(node, 1), t1);
    irSim << irSim.icArith(node, place, OP_ZERO, t1);
}

static void TR_CallFunc(ast *node, C_OP &place) {
    attr *f = findfunc(sibling(node, -1)->lex);
    assert(f);
    ast *s1 = sibling(node, 1);
    if (s1->type == RP) {
        if (string("read") == f->function->name)
            irSim << irSim.icRead(place);
        else
            irSim << irSim.icCall(place, f->function->name);
        return;
    }
    if ((string("write") == f->function->name)) {
        Operand a1 = TR_Args(child(s1, 1), true);
        irSim << irSim.icWrite(a1);
        return;
    }
    TR_Args(child(s1, 1));
    irSim << irSim.icCall(place, f->function->name);
}

static void TR_Exp(ast *node, C_OP &place) {
    ast *c2 = child(node, 2), *c1 = child(node, 1);
    if (c2) {
        switch (c2->type) {
            default:            TR_ARITH(c2, place);    break;
            case ASSIGNOP:      TR_ASSIGNOP(c2, place); break;
            case LP:            TR_CallFunc(c2, place); break;
            case Exp:
                switch (c1->type) {
                    case MINUS: TR_MINUS(c1, place);    break;
                    case LP:    TR_Exp(c2, place);      break;
                    case NOT:   TR_ARITH(c1, place);    break;
                    default:    assert(0);
                }
                break;
            case LB:  /* TODO: Array */  assert(0);     break;
            case DOT: /* TODO: Struct */ assert(0);     break;
        }
    }
    else {
        switch (c1->type) {
            case ID:  TR_ID(c1, place);  break;
            case INT: TR_INT(c1, place); break;
            default:  assert(0);         break;
        }
    }
}

///////////////////////////////////////// COND

static void TR_Cond(ast *node, C_OP &lt, C_OP &lf) {
    Operand t1, t2, label1;
    C_OP *a = &label1, *b = &lf;
    switch (node->type) {
        case NOT:
            TR_Cond(sibling(node, 1), lf, lt);
            break;
        case OR: // no break
            a = &lt;
            b = &label1;
        case AND:
            label1 = irSim.newLabel();
            TR_Cond(sibling(node, -1), *a, *b);
            irSim << irSim.icLabel(label1);
            TR_Cond(sibling(node, 1), lt, lf);
            break;
        case RELOP:
            t1 = irSim.newTmpVar();
            t2 = irSim.newTmpVar();
            TR_Exp(sibling(node, -1), t1);
            TR_Exp(sibling(node, 1), t2);
            irSim << irSim.icIfGoto(node, lt, t1, t2);
            break;
        default:
            t1 = irSim.newTmpVar();
            TR_Exp(node, t1);
            irSim << irSim.icIfGoto(IC_NE, lt, t1, OP_ZERO) << irSim.icGoto(lf);
    }
}

//////////////////////////////////////// FUNCTION
static Operand TR_Args(ast *node, bool isWrite) {
    Operand t1 = irSim.newTmpVar();
    ast *s2 = sibling(node, 2);
    TR_Exp(node, t1);
    if (s2) TR_Args(child(s2, 1), isWrite);
    if (!isWrite) irSim << irSim.icArg(t1);
    return t1;
}

/////////////////////////////////////// STATEMENT
static void TR_StmtList(ast *node) {
    ast *c1 = child(node, 1);
    if (!c1 || c1->type == NullNode) return;
    TR_Stmt(c1);
    TR_StmtList(child(node, 2));
}

static void TR_VarDec(ast *node) {
    ast *c1 = child(node, 1);
    assert(c1->type == ID);
    attr *a = findvar(c1->lex);
    a->basic->name = strdup(irSim.newVar().value.c_str());
}

static void TR_Dec(ast *node) {
    ast *c3 = child(node, 3);
    TR_VarDec(child(node, 1));
    if (c3) TR_ASSIGNOP(child(node, 2), OP_NONE);
}

static void TR_DecList(ast *node) {
    ast *c3 = child(node, 3);
    TR_Dec(child(node, 1));
    if (c3) TR_DecList(c3);
}

static void TR_Def(ast *node) {
    TR_DecList(child(node, 2));
}

static void TR_DefList(ast *node) {
    ast *c1 = child(node, 1);
    if (!c1 || c1->type == NullNode) return;
    TR_Def(c1);
    TR_DefList(child(node, 2));
}

static void TR_CompSt(ast *node) {
    TR_DefList(child(node, 2));
    TR_StmtList(child(node, 3));
}

static void TR_Stmt(ast *node) {
    ast *c1 = child(node, 1), *c7;
    Operand label1, label2, label3, t1;
    switch (c1->type) {
        case Exp:    TR_Exp(c1, OP_NONE); break;
        case CompSt: TR_CompSt(c1);       break;
        case RETURN:
            t1 = irSim.newTmpVar();
            TR_Exp(child(node, 2), t1);
            irSim << irSim.icReturn(t1);
            break;
        case IF:
            label1 = irSim.newLabel();
            label2 = irSim.newLabel();
            TR_Cond(child(node, 3), label1, label2);
            irSim << irSim.icLabel(label1);
            TR_Stmt(child(node, 5));
            if (!(c7 = child(node, 7))) { // without ELSE
                irSim << irSim.icLabel(label2);
                return;
            }
            label3 = irSim.newLabel();
            irSim << irSim.icGoto(label3) << irSim.icLabel(label2);
            TR_Stmt(c7);
            irSim << irSim.icLabel(label3);
            break;
        case WHILE:
            label1 = irSim.newLabel();
            label2 = irSim.newLabel();
            label3 = irSim.newLabel();
            irSim << irSim.icLabel(label1);
            TR_Cond(child(node, 3), label2, label3);
            irSim << irSim.icLabel(label2);
            TR_Stmt(child(node, 5));
            irSim << irSim.icGoto(label1) << irSim.icLabel(label3);
            break;
        default: assert(0); break;
    }
}
