#ifndef ERROR_H
#define ERROR_H

#include <err.h>

//#define NOYYERROR

#ifndef NOYYERROR
#define YYERROR_VERBOSE
#endif

extern int error_state;
extern int stderr_in_tty;
extern int stdout_in_tty;
extern const char *cur_file;

void init();

void restart();

int restorestdin();

void usage();

void yyerror(const char *msg, ...);

void lexerror(const char *msg, ...);

void syntaxerror(const char *msg, ...);

void reportpreyyerror();

void incerrorstate();

#define LEX_ERROR(MSG, ...) \
do { \
    lexerror(MSG, __VA_ARGS__); \
    incerrorstate(); \
} while (0)

#endif
