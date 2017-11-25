#ifndef TREE_H
#define TREE_H

union value {
	long long numval;
	double fpval;
	const char *typeval;
	char *idval;
};

struct ast {
	int type;
	const char* typename;
	union value* val;
	struct ast* left;
	struct ast* right;
	struct YYLTYPE* pos;
	const char* filename;
	/* Children never grow up if they depend too much on their parents. :-) */
	//struct ast* parent;
};

extern struct ast* ast_root;

/* Syntax node. Range [-10000, -1000). ErrorNode is -2. NullNode is -1 */
enum {
	Program = -10000, ExtDefList, ExtDef, ExtDecList, Specifier, StructSpecifier, OptTag, Tag,
	VarDec, FunDec, VarList, ParamDec, CompSt, StmtList, Stmt, DefList, Def, Dec, DecList, Exp, Args,
	ErrorNode = -2, NullNode = -1
};

void freeast(struct ast* root);

struct ast *allocast(int nodetype, const char* typename, struct YYLTYPE* pos, int num, ...);
struct ast *allocnumval(long long num);
struct ast *allocfpval(double fp);
struct ast *allocidval(char* id);
struct ast *alloctypeval(const char* type);

void printast(struct ast* root);

#define ALLOCAST(nt, pos, num, ...) allocast(nt, #nt, pos, num, __VA_ARGS__)
#define ALLOCNODE(nt) allocast(nt, #nt, NULL, 0)
#define ALLOCERRNODE(nt, pos) allocast(nt, #nt, pos, 0)

#endif
