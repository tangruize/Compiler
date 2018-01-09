//
// Created by tangruize 17-12-15.
//

#if COMPILER_VERSION >= 3

#include "ir.h"
#include "symbol.h"

#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>

using namespace std;

#define IMP_ME(str) do { cerr << str " not implemented!\n"; exit(EXIT_FAILURE); } while(false)

const char *output_file;

static const vector<string> relOp = {"==", "!=", "<", ">", "<=", ">="};
static const char *icFmt[] = {
/* IR_NONE */   "",
/* 1  IC_ADD */    "%s := %s + %s\n",
/* 2  IC_SUB */    "%s := %s - %s\n",
/* 3  IC_MUL */    "%s := %s * %s\n",
/* 4  IC_DIV */    "%s := %s / %s\n",
/* 5  IC_ASSIGN */ "%s := %s\n",
/* 6  IC_REF */    "%s := &%s\n",
/* 7  IC_DEREF */  "%s := *%s\n",
/* 8  IC_DEREF_L*/ "*%s := %s\n",
/* 9  IC_ARG */    "ARG %s\n",
/* 10 IC_ARGADDR */"ARG &%s\n",
/* 11 IC_RETURN */ "RETURN %s\n",
/* 12 IC_CALL */   "%s := CALL %s\n",
/* 13 IC_FUNC */   "FUNCTION %s :\n",
/* 14 IC_GOTO */   "GOTO %s\n",
/* 15 IC_DEC */    "DEC %s %s\n",
/* 16 IC_LABEL */  "LABEL %s :\n",
/* 17 IC_PARAM */  "PARAM %s\n",
/* 18 IC_READ */   "READ %s\n",
/* 19 IC_WRITE */  "WRITE %s\n",
/* 20 IC_EQ */     "IF %s == %s GOTO %s\n",
/* 21 IC_NE */     "IF %s != %s GOTO %s\n",
/* 22 IC_LT */     "IF %s < %s GOTO %s\n",
/* 23 IC_GT */     "IF %s > %s GOTO %s\n",
/* 24 IC_LE */     "IF %s <= %s GOTO %s\n",
/* 25 IC_GE */     "IF %s >= %s GOTO %s\n"
};


C_OP OP_ZERO = Operand(0);
C_OP OP_ONE = Operand(1);
C_OP OP_NONE;

void IrSim::buildBB() {
    int i = 0;
    for (auto it = irList.begin(); it != irList.end(); ++it, ++i) {
        int k = it->kind;
        if (k == IC_FUNC) {
            basicBlocks.push_back(i);
            functions.insert(i);
        }
        else if (isGoto(k)) {
            int labelNo = labels[it->target.value];
            labelInBB[labelNo] = (int)basicBlocks.size();
            basicBlocks.push_back(labelNo);
            basicBlocks.push_back(i + 1);
        }
    }
    basicBlocks.push_back((int)irList.size());
    std::sort(basicBlocks.begin(), basicBlocks.end(), [](int a, int b) { return a <= b; });
    auto last = std::unique(basicBlocks.begin(), basicBlocks.end());
    basicBlocks.erase(last, basicBlocks.end());
    assert(!basicBlocks.empty());

    for (auto &it : labelInBB)
        it.second = (int)(find(basicBlocks.begin(), basicBlocks.end(), it.first) - basicBlocks.begin());
    int size = (int)basicBlocks.size() - 1;
    edges.resize((size_t)size);
    for (i = 0; i < size; ++i) { // build edges
        InterCode &bbend = irList[basicBlocks[i + 1] - 1];
        InterCode &bbnext = irList[basicBlocks[i + 1]];
        if (isGoto(bbend.kind))
            edges[i].insert(labelInBB[labels[bbend.target.value]]);
        if (i + 1 != size && functions.count(basicBlocks[i + 1]) == 0) {
            if (bbnext.kind == IC_GOTO)
                edges[i].insert(labelInBB[labels[bbnext.target.value]]);
            else
                edges[i].insert(i + 1);
        }
    }
}

void IrSim::buildActive() {
    int size = (int)edges.size();
    use.clear();
//    lastUsages.clear();
    use.resize((size_t)size);
    for (int i = 0; i < size; ++i) {
        lastUsages.clear();
        for (int j = (int) basicBlocks[i], k = (int) basicBlocks[i + 1] - 1; k >= j; --k) {
            int kind = irList[k].kind;
            if (kind < IC_NONVAR || kind > IC_NONVAR_LAST) {
                if (kind != IC_DEREF_L)
                    unsetUsage(irList[k][0], i);
                else
                    setUsage(irList[k][0], i, k);
                setUsage(irList[k][1], i, k);
                setUsage(irList[k][2], i ,k);
            }
            else if (kind == IC_WRITE || kind == IC_ARG || kind == IC_ARGADDR || kind == IC_RETURN || kind == IC_CALL) {
                setUsage(irList[k][0], i, k);
            }
        }
    }
}

void IrSim::optimizeAssign() {
    isOpted = false;
    int size = (int)edges.size();
    for (int i = 0; i < size; ++i) {
        for (int k = (int) basicBlocks[i], j = (int) basicBlocks[i + 1] - 1; k <= j; ++k) {
            int kind = irList[k].kind;
            if (kind == IC_ASSIGN) {
                int m = irList[k].target.active;
                if (irList[k].target.value[0] == 'v')
                    m = -1;
                if (m > 0) {
                    string val = irList[k].target.value;
                    string right = irList[k].arg1.value;
                    kind = irList[k].arg1.kind;
                    while (m > 0) {
                        int n = 0;
                        for (; n < 3; ++n) {
                            if (irList[m][n].value == val) {
                                isOpted = true;
                                irList[m][n].value = right;
                                irList[m][n].kind = kind;
                                int active = irList[m][n].active;
                                irList[m].doArith();
                                m = active;
                                irList[m][n].active = 0;
                                break;
                            }
                        }
                        if (n == 3) {
//                            cerr << "debug: index val != val (should not happen, ignore optimizing)" << endl;
                            m = -1;
                        }
                    }
                }
//                else if (irList[k].target.active != 0 && irList[k].target.value[0] != 'v') {
//                    cerr << "debug: target < 0 (should not happen, ignore optimizing)" << endl;
//                }
                if (m == 0 && canDisable(irList[k].target.value, i)) {
                    irList[k].disable();
                    isOpted = true;
                }
            }
            else if (kind >= IC_ADD && kind <= IC_ASSIGN) {
                if (irList[k].doArith())
                    isOpted = true;
            }
        }
    }
}

void IrSim::optimize() {
    buildBB();
    do {
        buildActive();
        optimizeAssign();
    } while (isOpted);
}

void IrSim::doSetUsage(Operand &op, int blk, const int *i) {
    if (op.kind != OP_VARIABLE) return;
    auto it = lastUsages.find(op.value);
    auto use_it = use[blk].find(op.value);
    if (it != lastUsages.end()) op.setActive(it->second);
    else op.makeActive();
    if (i) { //right
        lastUsages[op.value] = *i;
        if (use_it == use[blk].end())
            use[blk][op.value] = true;
    }
    else { //left
        if (it != lastUsages.end())
            lastUsages.erase(it);
        if (use_it == use[blk].end())
            use[blk][op.value] = false;
    }
}

int IrSim::ast2ic(ast *node) const {
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
        default:       assert(false);
    }
}

const char *IrSim::print3(C_IC &i) const {
    if (i.arg2.kind == IR_NONE) return print2(i);
    if (i.kind < IC_RELOP)
        snprintf(pb, PBSIZE, icFmt[i.kind], i[0].str().c_str(), i[1].str().c_str(), i[2].str().c_str());
    else
        snprintf(pb, PBSIZE, icFmt[i.kind], i[1].str().c_str(), i[2].str().c_str(), i[0].str().c_str());
    return pb;
}

const char *IrSim::print2(C_IC &i) const {
    if (i.arg1.kind == IR_NONE) return print1(i);
    if (i.kind == IC_SUB)
        snprintf(pb, PBSIZE, icFmt[i.kind], i[0].str().c_str(), OP_ZERO.str().c_str(), i[1].str().c_str());
    else
        snprintf(pb, PBSIZE, icFmt[i.kind], i[0].str().c_str(), i[1].str().c_str());
    return pb;
}

const char *IrSim::print1(C_IC &i) const {
    snprintf(pb, PBSIZE, icFmt[i.kind], i[0].str().c_str());
    return pb;
}

void IrSim::printStream(ostream &out) const { for (auto &i: irList) out << print(i); }

bool IrSim::canDisable(const string &val, int exclude) {
    for (int i = (int)use.size() - 1; i >= 0; --i) {
        if (i != exclude) {
            auto it = use[i].find(val);
            if (it != use[i].end())
                return false;
        }
    }
    return true;
}

IrSim &IrSim::commitIc(C_IC &ic) {
    if (!ic.kind || !ic[0].kind)
        return *this;
    if (ic.kind == IC_ARG) {
        auto it = irList.rbegin();
        if (it != irList.rend() && it->kind == IC_REF && it->target.value == ic.target.value) {
            *it = icArgAddr(it->arg1);
            return *this;
        }
    }
    else if (ic.kind == IC_LABEL) {
        labels[ic.target.value] = (int)irList.size();
    }
    irList.push_back(ic);
    return *this;
}

InterCode IrSim::icArg(C_OP &t) {
    auto it = irList.rbegin();
    if (it != irList.rend() && it->kind == IC_REF && it->target.value == t.value) {
        InterCode ret = icArgAddr(it->arg1);
        irList.pop_back();
        return ret;
    }
    return icNoOp(IC_ARG, t);
}

bool IrSim::useLastArrayAddr() {
    if (irList.empty() || irList.rbegin()->kind != IC_DEREF)
        return false;
    irList.rbegin()->kind = IC_ASSIGN;
    return true;
}

static IrSim irSim;

bool InterCode::doArith() {
    if (kind >= IC_ADD && kind <= IC_DIV) {
        int a1, a2;
        bool isA1 = arg1.getInt(a1), isA2 = arg2.getInt(a2);
        if (arg2.kind == IR_NONE && kind == IC_SUB) {
            a1 = -a1;
            kind = IC_ASSIGN;
            return true;
        }
        else if (isA1 && isA2) {
            switch (kind) {
                case IC_ADD: a1 += a2; break;
                case IC_SUB: a1 -= a2; break;
                case IC_MUL: a1 *= a2; break;
                default:     a1 /= a2;
            }
            arg2.clear();
            arg1 = Operand(a1);
            kind = IC_ASSIGN;
            return true;
        }
        else if (isA1 || isA2) {
            if (isA1 && a1 == 0) {
                if (kind == IC_MUL || kind == IC_DIV) {
                    kind = IC_ASSIGN;
                    arg2.clear();
                    arg1 = OP_ZERO;
                }
                else if (kind == IC_ADD) {
                    kind = IC_ASSIGN;
                    arg1 = arg2;
                    arg2.clear();
                }
            }
            else if (isA2 && a2 == 0) {
                if (kind == IC_MUL) {
                    kind = IC_ASSIGN;
                    arg2.clear();
                    arg1 = OP_ZERO;
                }
                else if (kind == IC_ADD || kind == IC_SUB) {
                    kind = IC_ASSIGN;
                    arg2.clear();
                }
            }
        }
    }
    return false;
}

C_OP &InterCode::at(int i) const {
    switch(i) {
        case 1:  return arg1;
        case 2:  return arg2;
        default: return target;
    }
}

class Array {
private:
    deque<int> directSize;
    string newName;

public:
    Array() = default;
    Array(const string &name, attr *a) {
        newName = name;
        while (a->kind == ARRAY) {
            directSize.push_front(a->array->size);
            a = a->array->type;
        }
        if (a->kind == BASIC) directSize.push_front(4);
        else { /* TODO Struct */
            IMP_ME("Structure");
        }
        for (int i = 1; i < (int)directSize.size(); ++i)
            directSize[i] *= directSize[i - 1];
    }
    int getSize(int i) {
        assert(i < (int)directSize.size() && i >= 0);
        return directSize[i];
    }
    int getSize() { return *(directSize.rbegin()); }
    string getName() const { return newName; }
};
static map<string, Array> arrays;
static map<string, string> basics;

static const char *TR_VarDec(ast *node, int *size = nullptr);
static void TR_CompSt(ast *node);

extern "C" void genIR() {
#if COMPILER_VERSION >= 3
    ast *node = child(ast_root, 1);
    while (node && node->type != NullNode) {
        ast *ext = child(node, 1), *func = child(ext, 2), *id = child(func, 1), *var = child(func, 3);
        if (func->type != FunDec)
            IMP_ME("Global definition or structure");
        irSim << irSim.icFunction(id->lex);
        while (var && var->type == VarList) {
            irSim << irSim.icParam(TR_VarDec(child(child(var, 1), 2)));
            var = child(var, 3);
        }
        TR_CompSt(child(ext, 3));
        node = child(node, 2);
    }
//    irSim.printStream(cout);
#if COMPILER_VERSION == 3
    ofstream out(output_file + string(".orig.ir"));
    if (out.is_open()) {
        irSim.printStream(out);
        out.close();
    }
#endif

    irSim.optimize();

#if COMPILER_VERSION == 3
    out.open(output_file);
    if (out.is_open()) {
        irSim.printStream(out);
        out.close();
    }
#endif
#endif
}

//translate section:
static void TR_Exp(ast *node, C_OP &place);
static void TR_Cond(ast *node, C_OP &lt, C_OP &lf);
static void TR_Stmt(ast *node);
static Operand TR_Args(ast *node, bool isPush = true);

//////////////////////////////////////////// EXPR
static void TR_ID(ast *node, C_OP &place) {
    attr *a = findvar(node->lex);
    if (a->kind == BASIC) {
        irSim << irSim.icAssign(place, basics[node->lex]);
    }
    else if (a->kind == ARRAY) {
        string name = arrays[string(node->lex)].getName();
        if (irSim.isArg(name))
            irSim << irSim.icAssign(place, arrays[string(node->lex)].getName());
        else
            irSim << irSim.icRef(place, arrays[string(node->lex)].getName());
        irSim.setCurArray(node->lex);
    }
}

static void TR_INT(ast *node, C_OP &place) {
    irSim << irSim.icAssign(place, Operand(OP_INTCONST, node->lex));
}

static void TR_ASSIGNOP(ast *node, C_OP &place) {
    ast *pre_sibling1 = sibling(node, -1), *exp1_id_node = child(pre_sibling1, 1);
    Operand t1 = irSim.newTmpVar();
    if (exp1_id_node->type == ID) {
        TR_Exp(sibling(node, 1), t1);
        irSim << irSim.icAssign(basics[exp1_id_node->lex], t1) << irSim.icAssign(place, basics[exp1_id_node->lex]);
        return;
    }
    //ARRAY
    Operand t2 = irSim.newTmpVar();
    TR_Exp(sibling(node, 1), t2);
    TR_Exp(pre_sibling1, t1);
    if (!irSim.useLastArrayAddr()) assert(false);
    irSim << irSim.icDerefL(t1, t2) << irSim.icAssign(place, t1);
}

static void TR_ARITH(ast *node, C_OP &place) {
    int type = irSim.ast2ic(node);
    if (irSim.isGoto(type)) {
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
    ast *s1 = sibling(node, 1);
    if (s1->type == RP) {
        if (string("read") == f->function->name) irSim << irSim.icRead(place);
        else irSim << irSim.icCall(place, f->function->name);
        return;
    }
    if ((string("write") == f->function->name)) {
        Operand a1 = TR_Args(child(s1, 1), false);
        irSim << irSim.icWrite(a1);
        return;
    }
    TR_Args(child(s1, 1));
    Operand ret = place;
    if (place.kind == IR_NONE)
        ret = irSim.newTmpVar();
    irSim << irSim.icCall(ret, f->function->name);
}

static void TR_LB(ast *node, C_OP &place) { //node is LB
    Operand t1 = irSim.newTmpVar(), t2 = irSim.newTmpVar(), t3 = irSim.newTmpVar(), t4 = irSim.newTmpVar();
    irSim.incCurDim();
    int cd = irSim.getCurDim();
    TR_Exp(sibling(node, 1), t2);
    TR_Exp(sibling(node, -1), t1);
    const string &ca = irSim.getCurArray();
    irSim << irSim.icBinOp(IC_MUL, t3, t2, Operand(arrays[ca].getSize(cd)));
    irSim << irSim.icBinOp(IC_ADD, t4, t1, t3);
    if (cd) irSim << irSim.icAssign(place, t4);
    else {
        irSim << irSim.icDeref(place, t4);
        irSim.resetCurDim();
    }
}

static void TR_Exp(ast *node, C_OP &place) {
    ast *c2 = child(node, 2), *c1 = child(node, 1);
    if (c2) {
        if (c2->type != LB) irSim.resetCurDim();
        switch (c2->type) {
            default:            TR_ARITH(c2, place);    break;
            case ASSIGNOP:      TR_ASSIGNOP(c2, place); break;
            case LP:            TR_CallFunc(c2, place); break;
            case Exp:
                switch (c1->type) {
                    case MINUS: TR_MINUS(c1, place);    break;
                    case LP:    TR_Exp(c2, place);      break;
                    case NOT:   TR_ARITH(c1, place);    break;
                    default:    assert(false);
                }
                break;
            case LB:            TR_LB(c2, place);       break;
            case DOT: /* TODO: Struct */
                IMP_ME("Structure");
                break;
        }
    }
    else {
        switch (c1->type) {
            case ID:  TR_ID(c1, place);  break;
            case INT: TR_INT(c1, place); break;
            default:  assert(false);
        }
    }
}

static void TR_AND_OR(ast *node, C_OP &lt, C_OP &lf, bool isAnd = true) {
    Operand label1 = irSim.newLabel();
    if (isAnd) TR_Cond(sibling(node, -1), label1, lf);
    else       TR_Cond(sibling(node, -1), lt, label1);
    irSim << irSim.icLabel(label1);
    TR_Cond(sibling(node, 1), lt, lf);
}

///////////////////////////////////////// COND
static void TR_Cond(ast *node, C_OP &lt, C_OP &lf) {
    Operand t1, t2;
    switch (node->type) {
        case NOT:
            TR_Cond(sibling(node, 1), lf, lt);
            break;
        case OR: // no break
            TR_AND_OR(node, lt, lf, false);
            break;
        case AND:
            TR_AND_OR(node, lt, lf, true);
            break;
        case RELOP:
            t1 = irSim.newTmpVar();
            t2 = irSim.newTmpVar();
            TR_Exp(sibling(node, -1), t1);
            TR_Exp(sibling(node, 1), t2);
            irSim << irSim.icIfGoto(node, lt, t1, t2) << irSim.icGoto(lf);
            break;
        default:
            t1 = irSim.newTmpVar();
            TR_Exp(node, t1);
            irSim << irSim.icIfGoto(IC_NE, lt, t1, OP_ZERO) << irSim.icGoto(lf);
    }
}

//////////////////////////////////////// FUNCTION
static Operand TR_Args(ast *node, bool isPush) {
    Operand t1 = irSim.newTmpVar();
    ast *s2 = sibling(node, 2);
    TR_Exp(node, t1);
//    irSim.useLastArrayAddr();
    if (s2) TR_Args(child(s2, 1), isPush);
    if (isPush) irSim << irSim.icArg(t1);
    return t1;
}

/////////////////////////////////////// STATEMENT
static void TR_StmtList(ast *node) {
    ast *c1 = child(node, 1);
    if (!c1 || c1->type == NullNode) return;
    TR_Stmt(c1);
    TR_StmtList(child(node, 2));
}

static const char *TR_VarDec(ast *node, int *size) {
    ast *c1 = child(node, 1);
    while (c1->type != ID) c1 = child(c1, 1);
    attr *a = findvar(c1->lex);
    const char *name = strdup(irSim.newVar().value.c_str());
    if (a->kind == ARRAY) {
        arrays[c1->lex] = Array(name, a);
        if (size) *size = arrays[c1->lex].getSize();
    }
    else if (a->kind == BASIC) basics[c1->lex] = name;
    else assert(false);
    return name;
}

static void TR_Dec(ast *node) {
    ast *c3 = child(node, 3);
    int size = 0;
    const char *name = TR_VarDec(child(node, 1), &size);
    if (size) irSim << irSim.icDec(name, to_string(size));
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
            if (!(c7 = child(node, 7))) { // without ELSE node
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
        default: assert(false); break;
    }
}

#endif