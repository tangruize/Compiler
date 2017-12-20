%option yylineno
%{
#include "parser.h"
#include "error.h"
#include "tree.h"

#include <limits.h>
#include <math.h>
#include <string.h>

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

#define S_TEXT yylval.type_ast->lex = strdup(yytext)
#define L(nt) do { yylval.type_ast = allocast(nt, #nt, &yylloc, 0); S_TEXT; return nt; } while (0)

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
{INT}		{ yylval.type_ast = allocnumval(lexgetint(sizeof(int))); S_TEXT; return INT; }
{ILOCT}		{ lexilnum(1); S_TEXT; return INT; }
{ILHEX}		{ lexilnum(2); S_TEXT; return INT; }
{FLOAT}		{ lexfloat(); S_TEXT; return FLOAT; }
{ILFLO}		{ lexilnum(3); S_TEXT; return FLOAT; }
{SEMI}		{ L(SEMI); }
{COMMA}		{ L(COMMA); }
{ASSIGNOP}	{ L(ASSIGNOP); }
{RELOP}		{ L(RELOP); }
{PLUS}		{ L(PLUS); }
{MINUS}		{ L(MINUS); }
{STAR}		{ L(STAR); }
{DIV}		{ L(DIV); }
{AND}		{ L(AND); }
{OR}		{ L(OR); }
{DOT}		{ L(DOT); }
{NOT}		{ L(NOT); }
{LP}		{ L(LP); }
{RP}		{ L(RP); }
{LB}		{ L(LB); }
{RB}		{ L(RB); }
{LC}		{ L(LC); }
{RC}		{ L(RC); }
{TYPE}		{ lextype(); S_TEXT; return TYPE; }
{STRUCT}	{ L(STRUCT); }
{RETURN}	{ L(RETURN); }
{IF}		{ L(IF); }
{ELSE}		{ L(ELSE); }
{WHILE}		{ L(WHILE); }
{ID}		{ yylval.type_ast = allocidval(strdup(yytext)); S_TEXT; return ID; }
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
        "float", "double",
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

#undef L
#undef S_TEXT