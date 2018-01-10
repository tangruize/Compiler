#ifndef COMPILER_TREE_H
#define COMPILER_TREE_H

#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "parser.h"

union value {
    long long numval;
    double fpval;
    const char *typeval;
    char *idval;
};

struct ast {
    int type;
    const char *name;
    union value *val;
    struct ast *left;
    struct ast *right;
    struct ast *pre;
    struct YYLTYPE *pos;
    const char *filename;
    struct ast* parent;
    char *lex;
};

extern struct ast *ast_root;

/* Syntax node. Range [-10000, -1000). ErrorNode is -2. NullNode is -1 */
enum {
    Program = -10000, ExtDefList, ExtDef, ExtDecList, Specifier, StructSpecifier, OptTag, Tag,
    VarDec, FunDec, VarList, ParamDec, CompSt, StmtList, Stmt, DefList, Def, Dec, DecList, Exp, Args,
    ErrorNode = -2, NullNode = -1
};

/* basic type */
enum {
    CHAR_T = 0, SHORT_T, INT_T, LONG_T, LLONG_T, FLOAT_T, DOUBLE_T
};

void freeast(struct ast *root);

struct ast *allocast(int nodetype, const char *name, struct YYLTYPE *pos, int num, ...);

struct ast *allocnumval(long long num);

struct ast *allocfpval(double fp);

struct ast *allocidval(char *id);

struct ast *alloctypeval(const char *type);

void printast(struct ast *root);

#define ALLOCAST(nt, pos, num, ...) allocast(nt, #nt, pos, num, __VA_ARGS__)
#define ALLOCNODE(nt) allocast(nt, #nt, &yylloc, 0)
#define ALLOCERRNODE(nt, pos) allocast(nt, #nt, pos, 0)

struct ast *child(struct ast *a, int n);
struct ast *sibling(struct ast *a, int n);

#ifdef __cplusplus
}
#endif

#endif
