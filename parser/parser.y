%{
#include "lexer.h"
#include "error.h"
#include "tree.h"
%}

%union {
    struct ast *type_ast;
}

/* Declared tokens */
%token <type_ast> INT FLOAT SEMI COMMA ASSIGNOP RELOP PLUS MINUS STAR
%token <type_ast> DIV AND OR DOT NOT LP RP LB RB LC RC TYPE STRUCT
%token <type_ast> RETURN IF ELSE WHILE ID

%type <type_ast> Program ExtDefList ExtDef ExtDecList Specifier StructSpecifier OptTag Tag
%type <type_ast> VarDec FunDec VarList ParamDec CompSt StmtList Stmt DefList Def Dec DecList Exp Args

%left COMMA
%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right NOT
%left DOT LP RP LB RB

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

/* High-level Definitions */
Program: ExtDefList { $$ = ALLOCAST(Program, &@1, 1, $1); ast_root = $$; if (!error_state) printast($$); else reportpreyyerror(); }
	;
ExtDefList: ExtDef ExtDefList { $$ = ALLOCAST(ExtDefList, &@1, 2, $1, $2); }
	| %empty { $$ = ALLOCNODE(NullNode);}
	;
ExtDef: Specifier ExtDecList SEMI { $$ = ALLOCAST(ExtDef, &@1, 3, $1, $2, $3); }
	| Specifier SEMI { $$ = ALLOCAST(ExtDef, &@1, 2, $1, $2); }
	| Specifier FunDec CompSt { $$ = ALLOCAST(ExtDef, &@1, 3, $1, $2, $3); }
	| Specifier ExtDecList error SEMI { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, before '%s'", yytext); }
	| Specifier error SEMI { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, before '%s'", yytext); }
	| Specifier error FunDec CompSt { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, before '%s'", yytext); }
	| Specifier FunDec error CompSt { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, before '%s'", yytext); }
	| Specifier error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, before '%s'", yytext); }
	;
ExtDecList: VarDec { $$ = ALLOCAST(ExtDecList, &@1, 1, $1); }
	| VarDec COMMA ExtDecList { $$ = ALLOCAST(ExtDecList, &@1, 3, $1, $2, $3); }
	;

/* Specifiers */
Specifier: TYPE { $$ = ALLOCAST(Specifier, &@1, 1,  $1); }
	| StructSpecifier { $$ = ALLOCAST(Specifier, &@1, 1, $1); }
	;
StructSpecifier: STRUCT OptTag LC DefList RC { $$ = ALLOCAST(StructSpecifier, &@1, 5, $1, $2, $3, $4, $5); }
	| STRUCT Tag { $$ = ALLOCAST(StructSpecifier, &@1, 2, $1, $2); }
	;
OptTag: ID { $$ = ALLOCAST(OptTag, &@1, 1, $1); }
	| %empty { $$ = ALLOCNODE(NullNode); }
	;
Tag: ID { $$ = ALLOCAST(Tag, &@1, 1, $1); }
	;

/* Declarators */
VarDec: ID { $$ = ALLOCAST(VarDec, &@1, 1, $1); }
	| VarDec LB INT RB { $$ = ALLOCAST(VarDec, &@1, 4, $1, $2, $3, $4); }
	| VarDec LB error RB { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near '%s'", yytext); }
	;
FunDec: ID LP VarList RP { $$ = ALLOCAST(FunDec, &@1, 4, $1, $2, $3, $4); }
	| ID LP RP { $$ = ALLOCAST(FunDec, &@1, 3, $1, $2, $3); }
	| ID LP error RP { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, before '%s'", yytext); }
	;
VarList: ParamDec COMMA VarList { $$ = ALLOCAST(VarList, &@1, 3, $1, $2, $3); }
	| ParamDec { $$ = ALLOCAST(VarList, &@1, 1, $1); }
	;
ParamDec: Specifier VarDec { $$ = ALLOCAST(ParamDec, &@1, 2, $1, $2); }
	;

/* Statements */
CompSt: LC DefList StmtList RC { $$ = ALLOCAST(CompSt, &@1, 4, $1, $2, $3, $4); }
	| LC error DefList StmtList RC { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near '%s'", yytext); }
	;
StmtList: Stmt StmtList { $$ = ALLOCAST(StmtList, &@1, 2, $1, $2); }
	| %empty { $$ = ALLOCNODE(NullNode); }
	;
Stmt: Exp SEMI { $$ = ALLOCAST(Stmt, &@1, 2, $1, $2); }
	| CompSt { $$ = ALLOCAST(Stmt, &@1, 1, $1); }
	| RETURN Exp SEMI
	| IF LP Exp RP Stmt %prec LOWER_THAN_ELSE { $$ = ALLOCAST(Stmt, &@1, 5, $1, $2, $3, $4, $5); }
	| IF LP Exp RP Stmt ELSE Stmt { $$ = ALLOCAST(Stmt, &@1, 7, $1, $2, $3, $4, $5, $6, $7); }
	| WHILE LP Exp RP Stmt { $$ = ALLOCAST(Stmt, &@1, 5, $1, $2, $3, $4, $5); }
	| RETURN Exp error SEMI { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, before '%s'", yytext); }
	| RETURN error Exp SEMI { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near '%s'", yytext); }
	| Exp error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, before '%s'", yytext); }
	;

/* Local Definitions */
DefList: Def DefList { $$ = ALLOCAST(DefList, &@1, 2, $1, $2); }
	| %empty { $$ = ALLOCNODE(NullNode); }
	;
Def: Specifier DecList SEMI { $$ = ALLOCAST(Def, &@1, 3, $1, $2, $3); }
	| Specifier error DecList SEMI { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near '%s'", yytext); }
	| Specifier DecList error SEMI { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, before '%s'", yytext); }
	;
DecList: Dec { $$ = ALLOCAST(DecList, &@1, 1, $1); }
	| Dec COMMA DecList { $$ = ALLOCAST(DecList, &@1, 3, $1, $2, $3); }
	;
Dec: VarDec { $$ = ALLOCAST(Dec, &@1, 1, $1); }
	| VarDec ASSIGNOP Exp { $$ = ALLOCAST(Dec, &@1, 3, $1, $2, $3); }
	;

/* Expressions */
Exp: Exp ASSIGNOP Exp { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| Exp AND Exp { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| Exp OR Exp { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| Exp RELOP Exp { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| Exp PLUS Exp { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| Exp MINUS Exp { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| Exp STAR Exp { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| Exp DIV Exp { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| LP Exp RP { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| MINUS Exp { $$ = ALLOCAST(Exp, &@1, 2, $1, $2); }
	| NOT Exp { $$ = ALLOCAST(Exp, &@1, 2, $1, $2); }
	| ID LP Args RP { $$ = ALLOCAST(Exp, &@1, 4, $1, $2, $3, $4); }
	| ID LP RP { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| Exp LB Exp RB { $$ = ALLOCAST(Exp, &@1, 4, $1, $2, $3, $4); }
	| Exp DOT ID { $$ = ALLOCAST(Exp, &@1, 3, $1, $2, $3); }
	| ID { $$ = ALLOCAST(Exp, &@1, 1, $1); }
	| INT { $$ = ALLOCAST(Exp, &@1, 1, $1); }
	| FLOAT { $$ = ALLOCAST(Exp, &@1, 1, $1); }
	| Exp LB error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| Exp ASSIGNOP error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| Exp AND error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| Exp OR error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| Exp RELOP error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| Exp PLUS error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| Exp MINUS error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| Exp STAR error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| Exp DIV error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| ID LP error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	| LP error { $$ = ALLOCERRNODE(ErrorNode, &@1); syntaxerror("Syntax error, near %s", yytext); }
	;
Args: Exp COMMA Args { $$ = ALLOCAST(Args, &@1, 3, $1, $2, $3); }
	| Exp { $$ = ALLOCAST(Args, &@1, 1, $1); }
	;

%%
