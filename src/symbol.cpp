//
// Created by tangruize on 17-11-25.
//

#include "symbol.h"
#include <string>
#include <map>
#include <assert.h>

using namespace std;

class Env {
private:
    map<string, attr> var_table;
    map<string, attr> func_table;
    Env *pre;

public:
    Env(Env *p) {
        pre = p;
    }

    Env(): Env(NULL) {}

    struct attr* findvar(const string &s) {
        map<string, attr>::iterator found;
        for (Env *e = this; e != NULL; e = e->pre) {
            found = e->var_table.find(s);
            if (found != e->var_table.end())
                return &(found->second);
        }
        return NULL;
    }

    struct attr* findfunc(const string &s) {
        map<string, attr>::iterator found;
        for (Env *e = this; e != NULL; e = e->pre) {
            found = e->func_table.find(s);
            if (found != e->func_table.end())
                return &(found->second);
        }
        return NULL;
    }

    bool insertvar(const string &s, const struct attr &a) {
        if (var_table.find(s) != var_table.end())
            return false;
        var_table[s] = a;
        return true;
    }

    bool insertfunc(const string &s, const struct attr &a) {
        if (func_table.find(s) != func_table.end())
            return false;
        func_table[s] = a;
        return true;
    }

    class Env* getpre() const {
        return pre;
    }
};

static Env *current = NULL;

extern "C" {

void newenv() {
    current = new Env(current);
}

void deleteenv() {
    if (current == NULL) return;
    Env *pre = current->getpre();
    delete current;
    current = pre;
}

void deleteallenv() {
    while (current)
        deleteenv();
}

struct attr *findvar(const char *s) {
    return current->findvar(s);
}

struct attr *findfunc(const char *s) {
    return current->findfunc(s);
}

int insertvar(const char *s, const struct attr *a) {
    return current->insertvar(s, *a);
}

int insertfunc(const char *s, const struct attr *a) {
    return current->insertfunc(s, *a);
}

static void freefield(struct field_t *f) {
    struct field_t *p = f;
    while (p) {
        freeattr(p->type);
        f = p;
        p = p->next;
        free(f);
    }
}

static void freearg(struct arg_t *a) {
    struct arg_t *p = a;
    while (p) {
        freeattr(p->arg);
        a = p;
        p = p->next;
        free(a);
    }
}

void freeattr(struct attr *a) {
    switch (a->kind) {
        case ARRAY:
            freeattr(a->array->type);
            free(a->array);
            break;
        case STRUCTURE:
            freefield(a->structure->field);
            free(a->structure);
            break;
        case FUNCTION:
            freeattr(a->function->return_type);
            freearg(a->function->arg_list);
            free(a->function);
        default:
            abort();
    }
}

}