//
// Created by tangruize on 17-12-14.
//

#ifndef COMPILER_IR_H
#define COMPILER_IR_H

#include "version.h"

#if COMPILER_VERSION >= 3

#ifdef __cplusplus

#include "tree.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <algorithm>

using namespace std;

enum {IR_NONE = 0, OP_VARIABLE = 1, OP_INTCONST, OP_LABEL};
enum { IC_ADD = 1, IC_SUB, IC_MUL, IC_DIV,
    IC_ASSIGN, IC_REF, IC_DEREF, IC_DEREF_L,
    IC_NONVAR, IC_ARG = IC_NONVAR, IC_ARGADDR, IC_RETURN, IC_CALL,
    IC_FUNC, IC_GOTO, IC_DEC, IC_LABEL, IC_PARAM, IC_READ, IC_WRITE,
    IC_NONVAR_LAST = IC_WRITE,
    IC_RELOP, IC_EQ = IC_RELOP, IC_NE, IC_LT, IC_GT, IC_LE, IC_GE,
    IC_AND, IC_OR, IC_RELOP_LAST = IC_OR};

class Operand {
public:
    int kind;
    string value;
    int active; // neg: nonactive; 0: always active; pos: next usage
    Operand(): kind(IR_NONE), active(-1) {}
    Operand(string v): kind(OP_VARIABLE), value(std::move(v)), active(-1) {} // NOLINT
    Operand(const char *v):   kind(OP_VARIABLE), value(v), active(-1) {} // NOLINT
    Operand(int k, string v): kind(k), value(std::move(v)), active(-1) {}
    Operand(int intconst): kind(OP_INTCONST), value(to_string(intconst)), active(-1) {} // NOLINT
    string str() const { return kind != OP_INTCONST ? value : "#" + value; }
    bool getInt(int &i) const { if (kind == OP_INTCONST) i = atoi(value.c_str()); return kind == OP_INTCONST; }
    void clear() { kind = IR_NONE; value.clear(); }
    Operand &makeActive() { active = 0; return *this; }
    void setActive(int i) { active = i; }
    bool operator==(const Operand &rval) { return kind == rval.kind && value == rval.value; }
};

typedef const Operand C_OP;

class InterCode {
public:
    int kind;
    Operand target, arg1, arg2;
    explicit InterCode(int k = IR_NONE): kind(k) {}
    InterCode(int k, Operand t): kind(k), target(move(t)) {}
    InterCode(int k, Operand t, Operand a1): kind(k), target(move(t)), arg1(move(a1)) {}
    InterCode(int k, Operand t, Operand a1, Operand a2):
            kind(k), target(move(t)), arg1(move(a1)), arg2(move(a2)) {}
    C_OP &at(int i) const;
    C_OP &operator[](int i) const { return at(i); }
    Operand &operator[](int i) { return const_cast<Operand &>(at(i)); }
    bool doArith();
    void disable() { kind = IR_NONE; target.clear(); arg1.clear(); arg2.clear(); }
};
typedef const InterCode C_IC;

typedef vector<InterCode> IrList_t;

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
    IrList_t irList;
    set<string> argSet;

    //For array type
    string curArray;
    int curDim;

private: // for optimization
    vector<int> basicBlocks;
    set<int> functions;
    vector<set<int>> edges;
    vector<map<string, bool>> use; //false: assign before use, true: used
    map<string, int> labels;
    map<int, int> labelInBB;
    map<string, int> lastUsages;
    bool isOpted;
    void doSetUsage(Operand &op, int blk, const int *i = nullptr);
    void setUsage(Operand &op, int blk, int i) { doSetUsage(op, blk, &i); }
    void unsetUsage(Operand &op, int blk)         { doSetUsage(op, blk); }
    void optimizeAssign();
    bool canDisable(const string &val, int exclude);
    void buildBB();
    void buildActive();

public:
    IrSim(): tmpSeq(1), varSeq(1), labelSeq(1), curDim(-1) { pb = new char[PBSIZE]; isOpted = false; }
    ~IrSim() { delete[] pb; }
    Operand newTmpVar() { return Operand(OP_VARIABLE, tmpVarPrefix + to_string(tmpSeq++)); }
    Operand newLabel()  { return Operand(OP_LABEL,    labelPrefix  + to_string(labelSeq++)); }
    Operand newVar()    { return Operand(OP_VARIABLE, VarPrefix    + to_string(varSeq++)); }
    IrSim &commitIc(C_IC &ic);
    IrSim &operator<<(C_IC &ic) { return commitIc(ic); }

    int ast2ic(ast *node) const;

    InterCode icBinOp(int kind, C_OP &t, C_OP &a1, C_OP &a2) const { return InterCode(kind, t, a1, a2); }
    InterCode icUnaryOp(int kind, C_OP &t, C_OP &a1) const { return InterCode(kind, t, a1); }
    InterCode icNoOp(int kind, C_OP &t) const { return InterCode(kind, t); }

    InterCode icLabel(C_OP &t)    const { return icNoOp(IC_LABEL, t); }
    InterCode icFunction(C_OP &t) const { return icNoOp(IC_FUNC, t); }
    InterCode icReturn(C_OP &t)   const { return icNoOp(IC_RETURN, t); }
    InterCode icRead(C_OP &t)     const { return icNoOp(IC_READ, t); }
    InterCode icWrite(C_OP &t)    const { return icNoOp(IC_WRITE, t); }
    InterCode icGoto(C_OP &t)     const { return icNoOp(IC_GOTO, t); }
    InterCode icArgAddr(C_OP &t)  const { return icNoOp(IC_ARGADDR, t); }
    InterCode icParam(C_OP &t)    { argSet.insert(t.value); return icNoOp(IC_PARAM, t); }
    InterCode icArg(C_OP &t);

    InterCode icDec(C_OP &t, C_OP &a1)    const { return icUnaryOp(IC_DEC, t, a1); }
    InterCode icCall(C_OP &t, C_OP &a1)   const { return icUnaryOp(IC_CALL, t, a1); }
    InterCode icAssign(C_OP &t, C_OP &a1) const { return icUnaryOp(IC_ASSIGN, t, a1); }
    InterCode icDeref(C_OP &t, C_OP &a1)  const { return icUnaryOp(IC_DEREF, t, a1); }
    InterCode icDerefL(C_OP &t, C_OP &a1) const { return icUnaryOp(IC_DEREF_L, t, a1); }
    InterCode icRef(C_OP &t, C_OP &a1)    const { return icUnaryOp(IC_REF, t, a1); }

    InterCode icArith(ast *node, C_OP &t, C_OP &a1, C_OP &a2) const { return icBinOp(ast2ic(node), t, a1, a2); }
    InterCode icIfGoto(int kind, C_OP &t, C_OP &a1, C_OP &a2) const { return icBinOp(kind, t, a1, a2); }
    InterCode icIfGoto(ast *node, C_OP &t, C_OP &a1, C_OP &a2) const { return icIfGoto(ast2ic(node), t, a1, a2); }

    bool useLastArrayAddr();

    const char *print3(C_IC &i) const;
    const char *print2(C_IC &i) const;
    const char *print1(C_IC &i) const;
    const char *print(C_IC &i) const { return print3(i); }
    void printStream(ostream &out) const;

    void setCurArray(const string &s) { curArray = s; }
    string &getCurArray() { return curArray; }
    void incCurDim() { ++curDim; }
    void resetCurDim() { curDim = -1; }
    int &getCurDim() { return curDim; }

    bool isArg(const string &s) { return argSet.count(s) != 0; }
    bool isGoto(int k) { return k == IC_GOTO || (k >= IC_RELOP && k <= IC_RELOP_LAST); }

    void optimize();
    IrList_t &getIrList() { return irList; }
    void getFunctions(vector<int> &f);
};

extern IrSim irSim;

extern "C" {
#endif

void genIR();
extern const char *output_file;

#ifdef __cplusplus
}
#endif

#endif

#endif //COMPILER_IR_H
