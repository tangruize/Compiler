#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include "lexer.h"
#include "parser.h"
#include "error.h"
#include "tree.h"

static const char *error_tty_prefix = "\033[1;31m"; /* high-light red color */
static const char *error_tty_suffix = "\033[0m"; /* resume */
int error_state = 0;
int stderr_in_tty = 1;
int stdout_in_tty = 1;
int stdin_copy = -1;

const char *cur_file = NULL;

/* increase error state */
void incerrorstate() {
    ++error_state;
}

/* determine whether stdout is a tty or not and turn on yydebug */
void init() {
    if (!stdout || !isatty(fileno(stdout))) { /* not a tty, disable colors */
        error_tty_prefix = error_tty_suffix = "\0";
        stdout_in_tty = 0;
    }
    if (!stderr || !isatty(fileno(stderr)))
        stderr_in_tty = 0;
    stdin_copy = dup(STDIN_FILENO);
#if YYDEBUG == 1
    yydebug = 1;
#endif
}

void restart() {
    error_state = 0;
    yylineno = 1;
    freeast(ast_root);
    ast_root = NULL;
}

int restorestdin() {
    int ret = dup2(stdin_copy, STDIN_FILENO);
    if (ret != -1)
        rewind(stdin);
    return ret;
}

/* invoke program with argument '--help' */
void usage() {
    extern char *program_invocation_name;
    fprintf(stderr, "Usage: %s [CMM source]\n", program_invocation_name);
    exit(EXIT_FAILURE);
}

static int pre_yyerror_line = 0;

/* error type func */
static void typeerror(int which, const char *s, va_list ap) {
    int line = yylloc.first_line;
    const char *type;
    switch (which) {
        case 0:
            type = "A";
            break;
        case 1:
            if (!pre_yyerror_line)
                return;
            type = "B";
            line = pre_yyerror_line;
            pre_yyerror_line = 0;
            break;
        case 2:
            type = "B";
            break;
        default:
            abort();
    }
    fputs(error_tty_prefix, stdout);
    printf("Error type %s at Line %d: ", type, line);
    vprintf(s, ap);
    fputs(error_tty_suffix, stdout);
    putchar('\n');
}

#define FUNC_ERROR(NAME, TYPE) \
void NAME(const char* s, ...) { \
    va_list ap; \
    va_start(ap, s); \
    typeerror(TYPE, s, ap); \
    va_end(ap); \
}

/* error type A */ FUNC_ERROR(lexerror, 0)
/* error type B */ FUNC_ERROR(syntaxerror, 1)

static char pre_yyerror_buf[128];

void reportpreyyerror() {
#ifdef NOYYERROR
    if (pre_yyerror_line)
        syntaxerror(pre_yyerror_buf);
    pre_yyerror_line = 0;
#endif
}

/* store error msg in pre_yyerror_buf. if my parser.y does not check the error, output it */
void yyerror(const char *s, ...) {
    reportpreyyerror();

    va_list ap, __attribute__((unused)) aq; /* aq may be unused */
    va_start(ap, s);
    pre_yyerror_line = yylloc.first_line;
    incerrorstate();

#ifndef NOYYERROR
    va_copy(aq, ap);
    typeerror(2, s, aq);
    va_end(aq);
#endif

    vsnprintf(pre_yyerror_buf, 127, s, ap);
    if (islower(pre_yyerror_buf[0]))
        pre_yyerror_buf[0] = (char)toupper(pre_yyerror_buf[0]);
    va_end(ap);
}
