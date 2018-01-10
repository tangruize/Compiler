#include "version.h"
#include "tree.h"
#include "parser.h"
#include "error.h"

#include <stdlib.h>
#include <malloc.h>
#include <stdarg.h>
#include <assert.h>
#include <memory.h>

struct ast *ast_root;

struct ast *allocast(int nodetype, const char *name, struct YYLTYPE *pos, int num, ...) {
    va_list ap;
    va_start(ap, num);
    struct ast *node = malloc(sizeof(struct ast));
    memset(node, 0, sizeof(struct ast));
    if (!node)
        err(EXIT_FAILURE, "malloc");
    if (pos) {
        node->pos = malloc(sizeof(struct YYLTYPE));
        if (!node->pos)
            err(EXIT_FAILURE, "malloc");
        *(node->pos) = *pos;
    } else
        node->pos = NULL;
    node->type = nodetype;
    if (num != 0)
        node->left = va_arg(ap, struct ast*);
    else
        node->left = NULL;
    struct ast *p = node->left;
    while (--num > 0) {
        p->parent = node;
        p->right = va_arg(ap, struct ast*);
        if (p->right)
            p->right->pre = p;
        p = p->right;
    }
    va_end(ap);
    node->name = name;
    node->filename = cur_file;
    return node;
}

#define FUNC_ALLOC(NAME, TYPE, ARGTYPE) \
struct ast *alloc ## NAME(ARGTYPE arg) { \
    struct ast* node = malloc(sizeof(struct ast)); \
    memset(node, 0, sizeof(struct ast));\
    if (!node) \
        err(EXIT_FAILURE, "malloc"); \
    node->val = malloc(sizeof(union value)); \
    if (!node->val) \
        err(EXIT_FAILURE, "malloc"); \
    node->type = TYPE; \
    node->val->NAME = arg; \
    node->name = #TYPE; \
    node->filename = cur_file; \
    node->pos = malloc(sizeof(struct YYLTYPE)); \
    if (!node->pos) \
        err(EXIT_FAILURE, "malloc"); \
    *(node->pos) = yylloc; \
    return node; \
}

FUNC_ALLOC(numval, INT, long long)
FUNC_ALLOC(fpval, FLOAT, double)
FUNC_ALLOC(idval, ID, char*)
FUNC_ALLOC(typeval, TYPE, const char*)

static void printastdepth(struct ast *root, int depth, FILE *file) {
    if (root->type != NullNode) {
        if (!file)
            file = stdout;
        fprintf(file, "%*s%s", depth * 2, depth != 0 ? " " : "", root->name);
        if (root->val)
            switch (root->type) {
                case TYPE:
                    fprintf(file, ": %s", root->val->typeval);
                    break;
                case ID:
                    fprintf(file, ": %s", root->val->idval);
                    break;
                case INT:
                    fprintf(file, ": %ld", (long) root->val->numval);
                    break;
                case FLOAT:
                    fprintf(file, ": %lf", root->val->fpval);
                    break;
                default:
                    abort();
            }
        if (root->pos) {
            fprintf(file, " (%d)", root->pos->first_line);
        }
        fputc('\n', file);
    }
    if (root->left) printastdepth(root->left, depth + 1, file);
    if (root->right) printastdepth(root->right, depth, file);
}

void printast(struct ast *root) {
#if COMPILER_VERSION == 1
    printastdepth(root, 0, stdout);
#elif COMPILER_VERSION >= 3
    extern const char *output_file;
    char path_buf[1024];
    if (snprintf(path_buf, 1024, "%s.syntax", output_file) < 1024) {
        FILE *file = fopen(path_buf, "w");
        if (file != NULL) {
            printastdepth(root, 0, file);
        }
        fclose(file);
    }
#endif
}

void freeast(struct ast *root) {
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

struct ast *child(struct ast *a, int n) {
    struct ast *ret;
    if (a)
        ret = a->left;
    else
        return NULL;
    if (n > 0) {
        while (--n && ret)
            ret = ret->right;
    }
    else {
        n = -n;
        do {
            ret = ret->parent;
        } while (n-- && ret);
    }
    return ret;
}

struct ast *sibling(struct ast *a, int n) {
    struct ast *ret = a;
    if (n >= 0) {
        while (n-- && ret)
            ret = ret->right;
    }
    else {
        n = -n;
        while (n-- && ret)
            ret = ret->pre;
    }
    return ret;
}