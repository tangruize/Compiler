%option yylineno
%{
#include "parser.h"
#include "error.h"
#include "tree.h"

#include <limits.h>
#include <math.h>

int yycolumn = 1;

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;

static void lexcomment2();
static void lexilnum(int type);
static void lextype();
static void lexfloat();
static long long int lexgetint(int bytes);

%}

INT			(0[Xx][0-9A-Fa-f]+|0[0-7]*|[1-9][0-9]*)
FLOAT		((([0-9]*\.[0-9]+)|([0-9]+\.[0-9]*))([Ee][+-]?[0-9]+)?)
SEMI		;
COMMA		,
ASSIGNOP	=
RELOP		>|<|>=|<=|==|!=
PLUS		\+
MINUS		-
STAR		\*
DIV			\/
AND			&&
OR			\|\|
DOT			\.
NOT			!
LP			\(
RP			\)
LB			\[
RB			\]
LC			\{
RC			\}
TYPE		char|short|long[ ]long[ ]int|long[ ]long|long[ ]int|long|int|float|double
STRUCT		struct
RETURN		return
IF			if
ELSE		else
WHILE		while
ID			[A-Za-z_][0-9A-Za-z_]*

COMMENT1	\/\/.*
COMMENT2	\/\*
BLANK		[ \t]+

/* errors below */
ILOCT		0[0-9]+
ILHEX		0[Xx][0-9A-Za-z]+
ILFLO		[0-9]*\.[0-9]*[eE][+-]?([0-9]*[.]?[0-9]*)

%%

{COMMENT1}	{ }
{COMMENT2}	{ lexcomment2(); }
{INT}		{ yylval.type_ast = allocnumval(lexgetint(sizeof(int))); return INT; }
{ILOCT}		{ lexilnum(1); return INT; }
{ILHEX}		{ lexilnum(2); return INT; }
{FLOAT}		{ lexfloat(); return FLOAT; }
{ILFLO}		{ lexilnum(3); return FLOAT; }
{SEMI}		{ yylval.type_ast = ALLOCNODE(SEMI); return SEMI; }
{COMMA}		{ yylval.type_ast = ALLOCNODE(COMMA); return COMMA; }
{ASSIGNOP}	{ yylval.type_ast = ALLOCNODE(ASSIGNOP); return ASSIGNOP; }
{RELOP}		{ yylval.type_ast = ALLOCNODE(RELOP); return RELOP; }
{PLUS}		{ yylval.type_ast = ALLOCNODE(PLUS); return PLUS; }
{MINUS}		{ yylval.type_ast = ALLOCNODE(MINUS); return MINUS; }
{STAR}		{ yylval.type_ast = ALLOCNODE(STAR); return STAR; }
{DIV}		{ yylval.type_ast = ALLOCNODE(DIV); return DIV; }
{AND}		{ yylval.type_ast = ALLOCNODE(AND); return AND; }
{OR}		{ yylval.type_ast = ALLOCNODE(OR); return OR; }
{DOT}		{ yylval.type_ast = ALLOCNODE(DOT); return DOT; }
{NOT}		{ yylval.type_ast = ALLOCNODE(NOT); return NOT; }
{LP}		{ yylval.type_ast = ALLOCNODE(LP); return LP; }
{RP}		{ yylval.type_ast = ALLOCNODE(RP); return RP; }
{LB}		{ yylval.type_ast = ALLOCNODE(LB); return LB; }
{RB}		{ yylval.type_ast = ALLOCNODE(RB); return RB; }
{LC}		{ yylval.type_ast = ALLOCNODE(LC); return LC; }
{RC}		{ yylval.type_ast = ALLOCNODE(RC); return RC; }
{TYPE}		{ lextype(); return TYPE; }
{STRUCT}	{ yylval.type_ast = ALLOCNODE(STRUCT); return STRUCT; }
{RETURN}	{ yylval.type_ast = ALLOCNODE(RETURN); return RETURN; }
{IF}		{ yylval.type_ast = ALLOCNODE(IF); return IF; }
{ELSE}		{ yylval.type_ast = ALLOCNODE(ELSE); return ELSE; }
{WHILE}		{ yylval.type_ast = ALLOCNODE(WHILE); return WHILE; }
{ID}		{ yylval.type_ast = allocidval(strdup(yytext)); return ID; }
{BLANK}		{ }
\n			{ yycolumn = 1; }
.			{ LEX_ERROR("Mysterious character \"%s\"", yytext); }

%%
static void lexcomment2() {
    int c1 = 0, c2 = input();
    while (1) {
        if (c2 == EOF) {
            LEX_ERROR("%s", "unterminated comment");
            break;
        }
        if (c1 == '*' && c2 == '/')
            break;
        c1 = c2;
        c2 = input();
    }
}

static void lexilnum(int type) {
    const char *p = "";
    switch (type) {
        case 1:
            p = "octal";
            yylval.type_ast = allocnumval(LLONG_MAX);
            break;
        case 2:
            p = "hexadecimal";
            yylval.type_ast = allocnumval(LLONG_MAX);
            break;
        case 3:
            p = "floating point";
            yylval.type_ast = allocfpval(NAN);
            break;
    }
    LEX_ERROR("Illegal %s number '%s'", p, yytext);
}

static const char *const types[] = {
        "char", "short", "int", "long", "long long",
        "float", "double"
                "long int", "long long int",
};

#define NR_TYPES (sizeof(types) / sizeof(types[0]))

static void lextype() {
    unsigned i = 0;
    for (; i < NR_TYPES; ++i)
        if (strcmp(yytext, types[i]) == 0) {
            if (i <= 6)
                yylval.type_ast = alloctypeval(types[i]);
            else
                yylval.type_ast = alloctypeval(types[i - 4]);
        }
}

enum {
    CHAR_T = 0, SHORT_T, INT_T, LONG_T, LLONG_T, FLOAT_T, DOUBLE_T
};

int whichtype(const char *str) {
    unsigned i = 0;
    for (; i < NR_TYPES - 2; ++i)
        if (str == types[i]) /* try compare address */
            return (int) i;
    for (i = 0; i < NR_TYPES; ++i)
        if (strcmp(str, types[i]) == 0) {
            if (i <= 6)
                return (int) i;
            else
                return (int) (i - 4);
        }
    return -1;
}

static int isvalidint(int bytes, long long int val) {
    switch (bytes) {
        case 1:
            if (val < CHAR_MIN || val > CHAR_MAX) return 0;
        case 2:
            if (val < SHRT_MIN || val > SHRT_MAX) return 0;
        case 4:
            if (val < INT_MIN || val > INT_MAX) return 0;
    }
    return 1;
}

/* dectect whether INT constant overflow or not */
static long long int lexgetint(int bytes) {
    errno = 0;
    long long int result = strtoll(yytext, NULL, 0);
    if (errno == 0 && isvalidint(bytes, result))
        return result;
    LEX_ERROR("%s", "Integer constant is too large for its type");
    return LLONG_MAX;
}

static void lexfloat() {
    char *p = NULL;
    errno = 0;
    double d = strtof(yytext, &p);
    if (errno != 0 || (p && p[0] != 0)) {
        lexilnum(3);
        yylval.type_ast = allocfpval(NAN);
    }
    yylval.type_ast = allocfpval(d);
}
