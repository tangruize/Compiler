//
// Created by tangruize on 17-11-25.
//

#include "semantics.h"
#include "tree.h"
#include "error.h"
#include "parser.h"
#include "symbol.h"

#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>

static struct attr* SEM_Specifier(struct ast *node);
static void SEM_CompSt(struct ast *node, struct field_t *f);
static void SEM_Stmt(struct ast *node);

static const char *cur_func = NULL;

static int isequal(struct attr *a, struct attr *b) {
    if (!a || !b)
        return 0;
    //TODO: check type equality
    if (a->kind != b->kind)
        return 0;
    switch (a->kind) {
        case BASIC:
            break;
        case ARRAY:
            break;
        case STRUCTURE:
            break;
        case FUNCTION:
            break;
        default:
            abort();
    }
    return 1;
}

static struct field_t* SEM_VarDec(struct ast *node, struct attr *a, int ins) {
    struct ast *child1 = child(node, 1);
    if (child1->type == VarDec) {
        struct attr *a1 = malloc(sizeof(struct attr));
        a1->kind = ARRAY;
        a1->array = malloc(sizeof(struct array_t));
        a1->array->type = a;
        a1->array->size = (int)child(node, 3)->val->numval;
        return SEM_VarDec(child1, a1, ins);
    }
    if (ins && insertvar(child1->val->idval, a) == 0)  //TODO: error insert
        printf("error insert %s\n", child1->val->idval);
    struct field_t *f = malloc(sizeof(struct field_t));
    f->type = a;
    f->name = child1->val->idval;
    f->next = NULL;
    return f;
}

static struct field_t* SEM_Dec(struct ast *node, struct attr *a, int ins) {
    struct ast *child3 = child(node, 3);
    struct field_t *f = SEM_VarDec(child(node, 1), a, ins);
    if (child3) {
        //TODO: assign op
    }
    return f;
}

static struct field_t* SEM_DecList(struct ast *node, struct attr *a, int ins) {
    struct ast *child2 = child(node, 2);
    struct field_t *f = SEM_Dec(child(node, 1), a, ins);
    if (child2)
        f->next = SEM_DecList(child2, a, ins);
    return f;
}

static struct field_t* SEM_Def(struct ast *node, int ins) {
    struct ast *child1 = child(node, 1);
    struct ast *child2 = child(node, 2);
    struct attr *ret = SEM_Specifier(child1);
    return SEM_DecList(child2, ret, ins);
}

static struct field_t* SEM_DefList(struct ast *node, int ins) {
    if (node->type == NullNode)
        return NULL;
    struct field_t *f = SEM_Def(child(node, 1), ins), *p = f;
    while (p->next)
        p = p->next;
    p->next = SEM_DefList(child(node, 2), ins);
    return f;
}

static struct attr* SEM_STRUCT_OptTag(struct ast *node) {
    struct attr* ret = malloc(sizeof(struct attr));
    ret->kind = STRUCTURE;
    ret->structure = malloc(sizeof(struct struct_t));
    if (node->type == NullNode)
        ret->structure->name = NULL;
    else
        ret->structure->name = child(node, 1)->val->idval;
    ret->structure->field = SEM_DefList(sibling(node, 2), 0);
    return ret;
}

static struct attr* SEM_STRUCT_Tag(struct ast *node) {
    struct attr* ret = malloc(sizeof(struct attr));
    ret->kind = STRUCTURE;
    ret->structure = malloc(sizeof(struct struct_t));
    ret->structure->name = child(node, 1)->val->idval;
    ret->structure->field = NULL;
    return ret;
}

static struct attr* SEM_StructSpecifier(struct ast *node) {
    struct ast *child2 = child(node, 2);
    if (child2->type == OptTag)
        return SEM_STRUCT_OptTag(child2);
    else
        return SEM_STRUCT_Tag(child2);
}

static struct attr* SEM_TYPE(struct ast *node) {
    extern int whichtype(const char *);
    struct attr *ret = malloc(sizeof(struct attr));
    ret->kind = BASIC;
    ret->basic = whichtype(node->val->typeval);
    return ret;
}

static struct attr* SEM_Specifier(struct ast *node) {
    struct ast *child1 = child(node, 1);
    if (child1->type == TYPE)
        return SEM_TYPE(child1);
    else // StructSpecifier
        return SEM_StructSpecifier(child1);
}

static void SEM_ExtDecList(struct ast *node, struct attr *a) {
    SEM_VarDec(child(node, 1), a, 1);
    struct ast *child3 = child(node, 3);
    if (child3)
        SEM_ExtDecList(child3, a);
}

static void SEM_Ext_SEMI(struct attr *a) {
    if (a->kind == STRUCTURE && a->structure->name) {
        if (insertvar(a->structure->name, a) == 0) //TODO: error insert
            printf("error insert structure %s\n", a->structure->name);
    }
    else //TODO: might be an error
        printf("error insert type %d\n", a->kind);
}

static struct attr* SEM_ParamDec(struct ast *node, struct field_t **f) {
    struct attr *s = SEM_Specifier(child(node, 1));
    if (*f == NULL)
        *f = SEM_VarDec(child(node, 2), s, 0);
    else {
        struct field_t *p = *f;
        while (p->next)
            p = p->next;
        p->next = SEM_VarDec(child(node, 2), s, 0);
    }
    return (*f)->type;
}

static struct arg_t* SEM_VarList(struct ast *node, struct field_t **f) {
    struct arg_t *a = malloc(sizeof(struct arg_t));
    a->arg = SEM_ParamDec(child(node, 1), f);
    struct ast *child3 = child(node, 3);
    if (child3)
        a->next = SEM_VarList(child3, f);
    else
        a->next = NULL;
    return a;
}

static void SEM_FunDec(struct ast *node, struct attr *a, struct field_t **f) {
    struct ast *child3 = child(node, 3);
    struct attr *to_ins = malloc(sizeof(struct attr));
    to_ins->kind = FUNCTION;
    to_ins->function = malloc(sizeof(struct func_t));
    to_ins->function->return_type = a;
    to_ins->function->name = child(node, 1)->val->idval;
    cur_func = to_ins->function->name;
    if (child3->type != RP)
        to_ins->function->arg_list = SEM_VarList(child3, f);
    else
        to_ins->function->arg_list = NULL;
    if (insertfunc(to_ins->function->name, to_ins) == 0) //TODO: error insert func
        printf("error insert function %s\n", to_ins->function->name);
}

static struct attr* SEM_Exp(struct ast *node) {
    struct ast *child1 = child(node, 1);
    struct ast *child2 = child(node, 2);
    struct ast *child3 = child(node, 3);
    struct attr *a, *b;
    switch (child1->type) {
        case Exp:
            a = SEM_Exp(child1);
            if (child2->type != LB && child2->type != DOT) {
                b = SEM_Exp(child3);
                if (!isequal(a, b)) { //error not match
                    printf("error type not match\n");
                }
                return a;
            }
            else if (child2->type == LB) {
                if (a->kind != ARRAY) //TODO: error expected ARRAY
                    printf("error expected ARRAY\n");
                b = SEM_Exp(child3);
                if (b->kind != TYPE || b->basic != INT_T) //TODO: error expected int
                    printf("error expected int\n");
                //TODO: compose array type
                return a;
            }
            else {
                if (a->kind != STRUCTURE) //TODO: error expected struct
                    printf("error expected struct\n");
                //TODO: search each field for ID
                //TODO: pick up the field type
            }
            break;
        case NOT:
        case MINUS:
            return SEM_Exp(child2);
        case ID:
            if (!child3) {
                a = findvar(child1->val->idval);
                if (a == NULL) //TODO: error not found
                    printf("error var not found: %s\n", child1->val->idval);
                //TODO: cannot return NULL
                return a;
            }
            a = findfunc(child1->val->idval);
            if (a == NULL) //TODO: error func not decl
                printf("error func not found: %s\n", child1->val->idval);
            //TODO check arg type
            return a;
        case INT:
        case FLOAT:
            a = malloc(sizeof(struct attr));
            a->kind = BASIC;
            a->basic = child1->type == INT ? INT_T : FLOAT_T;
            return a;
        default:
            break;
    }
    abort();
}

static void SEM_RETURN(struct ast *node) {
    if (!isequal(findfunc(cur_func), SEM_Exp(node))) //TODO: error not match
        printf("error return type\n");
}

static void SEM_IF(struct ast *node) {
    struct attr *expr_type = SEM_Exp(sibling(node, 2));
    if (expr_type->kind != TYPE || expr_type->basic != INT_T) //TODO: check expr_type
        printf("error if/while condition type\n");
    SEM_Stmt(sibling(node, 4));
    struct ast *sibling6 = sibling(node, 6);
    if (sibling6)
        SEM_Stmt(sibling6);
}

static void SEM_WHILE(struct ast *node) {
    SEM_IF(node);
}


static void SEM_Stmt(struct ast *node) {
    struct ast *child1 = child(node, 1);
    switch (child1->type) {
        case Exp:
            SEM_Exp(child1);
            break;
        case CompSt:
            SEM_CompSt(child1, NULL);
            break;
        case RETURN:
            SEM_RETURN(sibling(child1, 1));
            break;
        case IF:
            SEM_IF(child1);
            break;
        case WHILE:
            SEM_WHILE(child1);
            break;
        default:
            abort();
    }
}

static void SEM_StmtList(struct ast *node) {
    if (node->type == NullNode)
        return;
    SEM_Stmt(child(node, 1));
    SEM_StmtList(child(node, 2));
}

static void SEM_CompSt(struct ast *node, struct field_t *f) {
    newenv();
    for (; f; f = f->next)
        insertvar(f->name, f->type);
    SEM_DefList(child(node, 2), 1);
    SEM_StmtList(child(node ,3));
    deleteenv();
}

static void SEM_ExtDef(struct ast *node) {
    struct attr *spec_type = SEM_Specifier(child(node, 1));
    struct ast *child2 = child(node, 2);
    struct field_t *f = NULL;
    switch (child2->type) {
        case ExtDecList:
            SEM_ExtDecList(child2, spec_type);
            break;
        case SEMI:
            SEM_Ext_SEMI(spec_type);
            break;
        case FunDec:
            SEM_FunDec(child2, spec_type, &f);
            SEM_CompSt(sibling(child2, 1), f);
            break;
        default:
            break;
    }
}

static void SEM_ExtDefList(struct ast *node) {
    if (node->type == NullNode)
        return;
    SEM_ExtDef(child(node, 1));
    SEM_ExtDefList(child(node, 2));
}

static void SEM_Program(struct ast *node) {
    newenv();
    SEM_ExtDefList(child(node, 1));
    deleteallenv();
}

void semchecker() {
    SEM_Program(ast_root);
}