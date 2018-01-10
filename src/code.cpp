//
// Created by tangruize on 18-1-9.
// very naive code-gen, but it works well
//

#include "version.h"

#if COMPILER_VERSION >= 4

#include "code.h"
#include "ir.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <functional>
using namespace std;

static const IrList_t *irList_p;
#define irList (*irList_p)
static ofstream out;
static const C_IC *curIC, *tmpIC;
static map<string, int> localVarsOff;
static int para, local, paraStack = 0;

/* Convention: t0: target, t1: arg1, t2: arg2 */

static inline string getSp(int i) { return to_string((local - i + paraStack) * 4) + "($sp)"; }
static inline void disableSave() { curIC = nullptr; }
static inline string getTarSp() { return getSp(localVarsOff[tmpIC->target.value]); }
static inline string constArg2() { return tmpIC->arg2.kind == OP_VARIABLE ? "" : tmpIC->arg2.value; }
static inline string ifgoto(const string &instr) { return instr + " $t1, $t2, " + tmpIC->target.value + "\n"; }

static string loadVariables() {
    int ignore[] = {IC_REF}, special[] = {IC_RETURN, IC_WRITE, IC_ARG};
    for (int i : ignore) if (curIC->kind == i) return "";
    string result{};
    int i = 1;
    for (int j : special) if (curIC->kind == j) i = 0;
    for (; i < 3; ++i) {
        C_OP &op = curIC->at(i);
        auto it = localVarsOff.find(op.value);
        if (it != localVarsOff.end())
            result += "lw $t" + to_string(i) + ", " + getSp(it->second) + "\n";
        else if (op.kind == OP_INTCONST)
            result += "li $t" + to_string(i) + ", " + op.value + "\n";
    }
    return result;
}

static string saveVariables() {
    if (!curIC) return "";
    auto it = localVarsOff.find(curIC->target.value);
    if (it == localVarsOff.end()) return "";
    return "sw $t0, " + getSp(it->second) + "\n";
}

static string arith(const string &instr, const string &val2) {
    if (val2.empty() || val2 == "-") return instr + " $t0, $t1, $t2\n";
    return "addi $t0, $t1, " + val2 + "\n";
}

static string arg(bool isAddr) {
    string result;
    if (isAddr) result = "la $t0, " + getTarSp() + "\n";
//    else result = "lw $t0, " + getTarSp() + "\n";
    disableSave();
    ++paraStack;
    return result + "addi $sp, $sp, -4\nsw $t0, 0($sp)\n";
}

static string getFuncName(const string &func) {
    string special[] = {"main", "read", "write"};
    for (auto &i: special) if (func == i) return func;
    return "func_" + func;
}

static string funCall(const string &func) {
    int i = paraStack;
    paraStack = 0;
    string saveRet = curIC == nullptr ? "" : "move $t0, $v0\n";
    return "addi $sp, $sp, -4\n" "sw $ra, 0($sp)\n" "jal " + getFuncName(func) + "\n" "lw $ra, 0($sp)\n"
           "addi $sp, $sp, " + to_string((i + 1) * 4) + "\n" + saveRet;
}

static std::function<string(void)> translateFuncTable[] = {
/* IR_NONE */      []{ disableSave(); return ""; },
/* 1  IC_ADD */    []{ return arith("add", constArg2()); },
/* 2  IC_SUB */    []{ return arith("sub", "-" + constArg2()); },
/* 3  IC_MUL */    []{ return arith("mul", ""); },
/* 4  IC_DIV */    []{ return "div $t1, t2\nmflo t0\n"; },
/* 5  IC_ASSIGN */ []{ return "move $t0, $t1\n"; },
/* 6  IC_REF */    []{ return "la $t0, " + getSp(localVarsOff[tmpIC->arg1.value]) + "\n"; }, //NOTE: may not used
/* 7  IC_DEREF */  []{ return "lw $t0, 0($t1)\n"; },
/* 8  IC_DEREF_L*/ []{ disableSave(); return "sw $t1, 0($t0)\n"; },
/* 9  IC_ARG */    []{ return arg(false); },
/* 10 IC_ARGADDR */[]{ return arg(true); },
/* 11 IC_RETURN */ []{ disableSave(); return "move $v0, $t0\naddi $sp, $sp, " + to_string(local * 4) + "\njr $ra\n"; },
/* 12 IC_CALL */   []{ return funCall(tmpIC->arg1.value); },
/* 13 IC_FUNC */   []{ disableSave(); string sp = local == 0 ? "": "addi $sp, $sp, -" + to_string(local * 4) + "\n";
                       return getFuncName(tmpIC->target.value) + ":\n" + sp;},
/* 14 IC_GOTO */   []{ disableSave(); return "j " + tmpIC->target.value + "\n"; },
/* 15 IC_DEC */    []{ disableSave(); return ""; },
/* 16 IC_LABEL */  []{ disableSave(); return tmpIC->target.value + ":\n"; },
/* 17 IC_PARAM */  []{ disableSave(); return ""; }, //Checked earlier
/* 18 IC_READ */   []{ return funCall("read"); },
/* 19 IC_WRITE */  []{ disableSave(); return "move $a0, $t0\n" + funCall("write"); },
/* 20 IC_EQ */     []{ return ifgoto("beq"); },
/* 21 IC_NE */     []{ return ifgoto("bne"); },
/* 22 IC_LT */     []{ return ifgoto("blt"); },
/* 23 IC_GT */     []{ return ifgoto("bgt"); },
/* 24 IC_LE */     []{ return ifgoto("ble"); },
/* 25 IC_GE */     []{ return ifgoto("bge"); },
};

static void GEN_common(ostream &out) {
    out << ".data\n" "_prompt: .asciiz \"Enter an integer:\"\n" "_ret: .asciiz \"\\n\"\n"
            ".globl main\n" ".text\n" "\nread:\n" "li $v0, 4\n" "la $a0, _prompt\n" "syscall\n"
            "li $v0, 5\n" "syscall\n" "jr $ra\n" "\nwrite:\n" "li $v0, 1\n" "syscall\n" "li $v0, 4\n"
            "la $a0, _ret\n" "syscall\n" "move $v0, $0\n" "jr $ra\n";
}

static void GEN_function(int start, int end) { // end not included
    localVarsOff.clear();
    para = 0, local = 0;
    map<string, int> array;
    int i;
    for (i = start + 1; i < end; ++i) {
        if (irList[i].kind == IR_NONE) continue;
        else if (irList[i].kind == IC_PARAM) localVarsOff[irList[i].target.value] = --para;
        else if (irList[i].kind == IC_DEC) array[irList[i].target.value] = atoi(irList[i].arg1.value.c_str()) / 4 - 1;
        else break;
    }
    for (; i < end; ++i) {
        for (int j = 0; j < 3; ++j) {
            C_OP &op = irList[i][j];
            if (!op.value.empty() && (op.value[0] == 'v' || op.value[0] == 't'))
                if (localVarsOff.find(op.value) == localVarsOff.end()) {
                    localVarsOff[op.value] = ++local;
                    if (array.find(op.value) != array.end())
                        local += array[op.value];
                }
        }
    }
    for (i = start; i < end; ++i) {
        curIC = tmpIC = &(irList[i]);
        string s = irSim.print(*curIC);
        if (!s.empty()) out << "# " << s;
        out << loadVariables();
        out << translateFuncTable[tmpIC->kind]();
        out << saveVariables();
    }
}

extern "C" void genCode() {
    out.open(output_file);
    if (!out.is_open()) {
        cerr << "ERROR: Cannot open " << output_file << endl;
        return;
    }
    GEN_common(out);
    vector<int> functions;
    irList_p = &irSim.getIrList();
    irSim.getFunctions(functions);
    int sz = (int)functions.size() - 1;
    out << "\n#### GENERATE START ####\n";
    for (int i = 0; i < sz; ++i) {
        GEN_function(functions[i], functions[i + 1]);
        out << "\n";
    }
}

#endif