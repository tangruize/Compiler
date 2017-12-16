#include "ir.h"
#include "parser.h"
#include "lexer.h"
#include "error.h"
#include "semantics.h"

#if COMPILER_VERSION >= 3
const char *output_file;
#endif

int main(int argc, char *argv[]) {
#if COMPILER_VERSION >= 3
    if (argc < 3)
        usage();
    FILE *infile = fopen(argv[1], "r");
    if (!infile)
        err(EXIT_FAILURE, "%s", argv[1]);
    output_file = argv[2];
    yyrestart(infile);
    yyparse();
    if (!error_state)
        semchecker();
    if (!error_state)
        genIR();
#else
    init();
    FILE *infile = stdin; /* use stdin if no arg supplied */
    cur_file = "stdin";
    int i;
    for (i = 1; i < argc; i++) {
        cur_file = argv[i];
        if (!(infile = fopen(argv[i], "r"))) {
            if (strcmp(argv[i], "--help") == 0)
                usage();
            else if (strcmp(argv[i], "-") == 0) {
                if (restorestdin() == -1)
                    continue;
                cur_file = "stdin";
                infile = stdin;
            } else {
                warn("%s", argv[i]);
                if (argc != 2) fputc('\n', stderr);
                continue;
            }
        }
        yyrestart(infile);
        fflush(stdout);
        fflush(stderr);
        if (argc != 2) {
            printf("%sFile name: %c%s%c%s\n",
                   stdout_in_tty ? "\033[1m" : "",
                   infile != stdin ? '"' : '(', cur_file,
                   infile != stdin ? '"' : ')',
                   stdout_in_tty ? "\033[0m" : "");
        }
        yyparse();
        if (!error_state)
            semchecker();
        if (argc != 2) putchar('\n');
        if (infile != stdin) fclose(infile);
        infile = NULL;
        restart();
    }
    if (infile) { /* no args, read stdin */
        yyrestart(infile);
        yyparse();
    }
#endif
    return 0;
}


