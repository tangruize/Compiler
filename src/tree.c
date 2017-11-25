#include "tree.h"
#include "parser.h"
#include "error.h"

#include <stdlib.h>
#include <malloc.h>
#include <err.h>
#include <stdarg.h>
#include <string.h>

struct ast* ast_root;

struct ast *allocast(int nodetype, const char* typename, struct YYLTYPE* pos, int num, ...) {
	va_list ap;
	va_start(ap,num);
	struct ast* node = malloc(sizeof(struct ast));
	if (!node)
		err(EXIT_FAILURE, "malloc");
	if (pos) {
		node->pos = malloc(sizeof(struct YYLTYPE));
		if (!node->pos)
			err(EXIT_FAILURE, "malloc");
		*(node->pos) = *pos;
	}
	else
		node->pos = NULL;
	node->type = nodetype;
	if (num != 0)
		node->left = va_arg(ap, struct ast*);
	else
		node->left = NULL;
	struct ast* p = node->left;
	while (--num > 0) {
		p->right = va_arg(ap, struct ast*);
		p = p->right;
	}
	va_end(ap);
	node->right = NULL;
	node->val = NULL;
	node->typename = typename;
	node->filename = cur_file;
	return node;
}

#define FUNC_ALLOC(NAME, TYPE, ARGTYPE) \
struct ast *alloc ## NAME(ARGTYPE arg) { \
	struct ast* node = malloc(sizeof(struct ast)); \
	if (!node) \
		err(EXIT_FAILURE, "malloc"); \
	node->val = malloc(sizeof(union value)); \
	if (!node->val) \
		err(EXIT_FAILURE, "malloc"); \
	node->pos = NULL; \
	node->type = TYPE; \
	node->val->NAME = arg; \
	node->typename = #TYPE; \
	node->filename = cur_file; \
	node->left = node->right = NULL; \
	return node; \
}

FUNC_ALLOC(numval, INT, long long)
FUNC_ALLOC(fpval, FLOAT, double)
FUNC_ALLOC(idval, ID, char*)
FUNC_ALLOC(typeval, TYPE, const char*)

static void printastdepth(struct ast* root, int depth) {
	if (root->type != NullNode) {
		printf("%*s%s", depth * 2, depth != 0 ? " " : "", root->typename);
		if (root->val)
			switch (root->type) {
				case TYPE: printf(": %s", root->val->typeval); break;
				case ID: printf(": %s", root->val->idval); break;
				case INT: printf(": %lld", root->val->numval); break;
				case FLOAT: printf(": %lf", root->val->fpval); break;
			}
		if (root->pos) {
			printf(" (%d)", root->pos->first_line);
		}
		putchar('\n');
	}
	if (root->left) printastdepth(root->left, depth + 1);
	if (root->right) printastdepth(root->right, depth);
}

void printast(struct ast* root) {
	printastdepth(root, 0);
}

void freeast(struct ast* root) {
	if (!root) return;
	if (root->left) freeast(root->left);
	root->left = NULL;
	if (root->right) freeast(root->right);
	root->right = NULL;
	if (root->val) {
		if (root->type == ID) {
			free(root->val->idval);
			root->val->idval = NULL;
		}
		free(root->val);
		root->val = NULL;
	}
	if (root->pos) {
		free(root->pos);
		root->pos = NULL;
	}
	free(root);
}
