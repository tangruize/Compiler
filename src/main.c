#include "parser.h"
#include "lexer.h"
#include "error.h"
#include "tree.h"

int main(int argc, char* argv[]) {
	init();
	FILE *infile = stdin; /* use stdin if no arg supplied */
	cur_file = "stdin";
	int i;
	for(i = 1; i < argc; i++) {
		cur_file = argv[i];
		if (!(infile = fopen(argv[i], "r"))) {
			if (strcmp(argv[i], "--help") == 0)
				usage();
			else if (strcmp(argv[i], "-") == 0) {
				if (restorestdin() == -1)
					continue;
				cur_file = "stdin";
				infile = stdin;
			}
			else {
				warn("%s", argv[1]);
				putchar('\n');
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
		if (argc != 2) putchar('\n');
		if (infile != stdin) fclose(infile);
		infile = NULL;
		restart();
	}
	if (infile) { /* no args, read stdin */
		yyrestart(infile);
		yyparse();
	}
	return 0;
}
