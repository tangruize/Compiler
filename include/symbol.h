//
// Created by tangruize on 17-11-25.
//

#ifndef COMPILER_SYMBOL_H
#define COMPILER_SYMBOL_H

enum { BASIC, ARRAY, STRUCTURE, FUNCTION};

struct array_t {
    struct attr *type;
    int size;
};

struct field_t {
    const char *name;
    struct attr *type;
    struct field_t *next;
};

struct struct_t {
    const char *name;
    struct field_t *field;
};

struct arg_t {
    struct attr *arg;
    struct arg_t *next;
};

struct func_t {
    const char *name;
    struct attr *return_type;
    struct arg_t *arg_list;
};

struct attr {
    int kind;
    union {
        int basic;
        struct array_t *array;
        struct struct_t *structure;
        struct func_t *function;
    };
};

#ifdef __cplusplus
extern "C" {
#endif

void newenv();
void deleteenv();
void deleteallenv();

struct attr *findvar(const char *s);
struct attr *findfunc(const char *s);

int insertvar(const char *s, const struct attr *a);
int insertfunc(const char *s, const struct attr *a);

void freeattr(struct attr *a);

#ifdef __cplusplus
}
#endif

#endif //COMPILER_SYMBOL_H
