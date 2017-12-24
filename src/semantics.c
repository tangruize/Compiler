//
// Created by tangruize on 17-11-25.
//

#if COMPILER_VERSION >= 2

#include "semantics.h"

#include "tree.h"
#include "error.h"
#include "parser.h"
#include "symbol.h"

#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

static struct attr* SEM_Specifier(struct ast *node);
static void SEM_CompSt(struct ast *node, struct field_t *f);
static void SEM_Stmt(struct ast *node);

static struct attr *SEM_Exp(struct ast *node);

static const char *cur_func = NULL;

static int isequal(struct attr *a, struct attr *b) {
    if (!a || !b)
        return 0;
    int d1[10], d2[10];
    int depth1 = 0, depth2 = 0;
    for (; a->kind == ARRAY; ++depth1) {
        d1[depth1] = a->array->size;
        a = a->array->type;
    }
    for (; b->kind == ARRAY; ++depth2) {
        d2[depth2] = b->array->size;
        b = b->array->type;
    }
    if (depth1 != depth2)
        return 0;
    int i;
    for (i = 0; i < depth1; ++i) {
        if (d1[i] != d2[i])
            return 0;
    }
    if (a->kind == FUNCTION)
        a = a->function->return_type;
    if (b->kind == FUNCTION) {
        b = b->function->return_type;
    }
    //DONETODO: check type equality
    if (a->kind != b->kind)
        return 0;
    switch (a->kind) {
        case BASIC:
            if (a->basic->type != b->basic->type)
                return 0;
            break;
        case STRUCTURE:
            if (strcmp(a->structure->name, b->structure->name) != 0) {
                return 0;
            }
            break;
        case ARRAY:
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
    struct attr *sa = a;
    if (child1->type == VarDec) {
        struct attr *a1 = malloc(sizeof(struct attr));
        a1->kind = ARRAY;
        a1->array = malloc(sizeof(struct array_t));
        a1->array->type = a;
        a1->array->size = (int)child(node, 3)->val->numval;
        return SEM_VarDec(child1, a1, ins);
    }
    //DONETODO: error insert
    if (a->kind == STRUCTURE && a->structure->field == NULL && a->structure->name &&
        ((sa = findvar(a->structure->name)) == NULL)) {
//            printf("no such struct\n");
        SEMERROR(UndefinedStruct, node, a->structure->name);
        sa = a;
    }
    if (ins) {
        if (insertvar(child1->val->idval, sa) == 0)
            SEMERROR(RedefinedVariable, child1, child1->val->idval);
    }
    struct field_t *f = malloc(sizeof(struct field_t));
    f->isInit = 0;
    f->type = sa;
    f->name = child1->val->idval;
    f->next = NULL;
    return f;
}

static struct field_t* SEM_Dec(struct ast *node, struct attr *a, int ins) {
    struct ast *child3 = child(node, 3);
    struct field_t *f = SEM_VarDec(child(node, 1), a, ins);
    if (child3) {
        //DONETODO: assign op for var dec
        if (!isequal(f->type, SEM_Exp(child3))) {
//            printf("equal error\n");
            SEMERROR_NOVA(AssignMismatch, child3);
        } else
            f->isInit = 1;
    }
    return f;
}

static struct field_t* SEM_DecList(struct ast *node, struct attr *a, int ins) {
    struct ast *child3 = child(node, 3);
    struct field_t *f = SEM_Dec(child(node, 1), a, ins);
    if (child3)
        f->next = SEM_DecList(child3, a, ins);
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
    struct field_t *f = SEM_Def(child(node, 1), ins), *p = f, *q;
    while (p->next) {
        p = p->next;
    }
    p->next = SEM_DefList(child(node, 2), ins);
    p = p->next;
    if (!ins) {
        while (p) {
            q = f;
            while (q != p) {
                if (strcmp(p->name, q->name) == 0) {
                    SEMERROR(RedefinedField, child(node, 2), p->name);
                }
                q = q->next;
            }
            p = p->next;
        }
    }
    return f;
}

static struct attr* SEM_STRUCT_OptTag(struct ast *node) {
    struct attr* ret = malloc(sizeof(struct attr));
    ret->kind = STRUCTURE;
    ret->structure = malloc(sizeof(struct struct_t));
    ret->structure->field = SEM_DefList(sibling(node, 2), 0);
    struct field_t *f = ret->structure->field;
    for (; f; f = f->next) {
        if (f->isInit) {
            char buf[64];
            if (node->type == NullNode)
                sprintf(buf, "Initialized variable \"%s\" in structure.", f->name);
            else
                sprintf(buf, "Initialized variable \"%s\" in structure \"%s\".", f->name, child(node, 1)->val->idval);
            SEMERROR(OtherError, node, buf);
        }
    }
    if (node->type == NullNode)
        ret->structure->name = NULL;
    else {
        ret->structure->name = child(node, 1)->val->idval;
        if (!insertvar(ret->structure->name, ret)) {
            SEMERROR(DuplicatedStructName, child(node, 1), ret->structure->name);
        }
    }
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
    if (child2->type == OptTag || child2->type == NullNode)
        return SEM_STRUCT_OptTag(child2);
    else
        return SEM_STRUCT_Tag(child2);
}

static struct attr* SEM_TYPE(struct ast *node) {
    extern int whichtype(const char *);
    struct attr *ret = malloc(sizeof(struct attr));
    ret->kind = BASIC;
    ret->basic = malloc(sizeof(struct basic_t));
    ret->basic->name = node->val->typeval;
    ret->basic->type = whichtype(node->val->typeval);
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

static void SEM_Ext_SEMI(struct attr *a, struct ast *node) {
//    if (a->kind == STRUCTURE && a->structure->name) {
//        if (insertvar(a->structure->name, a) == 0) { //DONETODO: error insert
//            printf("error insert structure %s\n", a->structure->name);
//            SEMERROR(DuplicatedStructName, node, a->structure->name);
//        }
//    }
}

static struct attr* SEM_ParamDec(struct ast *node, struct field_t **f) {
    struct attr *s = SEM_Specifier(child(node, 1));
    struct field_t *p = *f;
    if (*f == NULL) {
        p = SEM_VarDec(child(node, 2), s, 0);
        *f = p;
    }
    else {
        while (p->next)
            p = p->next;
        p->next = SEM_VarDec(child(node, 2), s, 0);
        p = p->next;
    }
    return p->type;
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
    if (a->kind == STRUCTURE) {
        struct attr *sa = findvar(a->structure->name);
        if (sa)
            a = sa;
        else
            SEMERROR(UndefinedVariable, node, a->structure->name);
    }
    to_ins->function->return_type = a;
    to_ins->function->name = child(node, 1)->val->idval;
    cur_func = to_ins->function->name;
    if (child3->type != RP)
        to_ins->function->arg_list = SEM_VarList(child3, f);
    else
        to_ins->function->arg_list = NULL;
    if (insertfunc(to_ins->function->name, to_ins) == 0) {//DONETODO: error insert func
//        printf("error insert function %s\n", to_ins->function->name);
        SEMERROR(RedefinedFunction, node, to_ins->function->name);
    }
}

static void isFuncMatchCpStr(struct attr *exp, char *buf) {
    if (!exp)
        return;
    switch (exp->kind) {
        case BASIC:
            switch (exp->basic->type) {
                case INT_T:
                    strcat(buf, "int");
                    break;
                case FLOAT_T:
                    strcat(buf, "float");
                    break;
                default:
                    break;
            }
            break;
        case STRUCTURE:
            strcat(buf, "struct ");
            strcat(buf, exp->structure->name);
            break;
        case ARRAY: {
            char buf2[32];
            isFuncMatchCpStr(exp->array->type, buf);
            sprintf(buf2, "[%d]", exp->array->size);
            strcat(buf, buf2);
            break;
        }
        case FUNCTION:
            isFuncMatchCpStr(exp->function->return_type, buf);
            break;
        default:
            //DONETODO: check more type
            break;
    }
}

struct arg_t *SEM_Args(struct ast *node) {
    struct arg_t *a = malloc(sizeof(struct arg_t));
    a->arg = SEM_Exp(child(node, 1));
    struct ast *child3 = child(node, 3);
    if (child3)
        a->next = SEM_Args(child3);
    else
        a->next = NULL;
    return a;
}

static int isFuncMatch(struct ast *node, struct attr *a, char **s1, char **s2) {
    char *buf1 = malloc(256), *buf2 = malloc(256);
    int fun_name_len = (int) strlen(node->val->idval);
    sprintf(buf1, "%s(", node->val->idval);
    strcpy(buf2, "(");
    struct ast *args = sibling(node, 2);
    if (args->type != RP) {
        struct arg_t *arg1 = SEM_Args(args);
        for (; arg1; arg1 = arg1->next) {
            isFuncMatchCpStr(arg1->arg, buf2);
            strcat(buf2, ", ");
        }
    }
    for (struct arg_t *arg2 = a->function->arg_list; arg2; arg2 = arg2->next) {
        isFuncMatchCpStr(arg2->arg, buf1);
        strcat(buf1, ", ");
    }
    if (strcmp(buf1 + fun_name_len, buf2) != 0) {
        *s1 = buf1;
        *s2 = buf2;
        int len1 = (int) strlen(buf1);
        int len2 = (int) strlen(buf2);
        if (buf1[len1 - 1] != '(') {
            buf1[len1 - 2] = 0;
        }
        if (buf2[len2 - 1] != '(') {
            buf2[len2 - 2] = 0;
        }
        strcat(buf1, ")");
        strcat(buf2, ")");
        return 0;
    } else {
        free(buf1);
        free(buf2);
        return 1;
    }
}

static struct attr* SEM_Exp(struct ast *node) {
    struct ast *child1 = child(node, 1);
    struct ast *child2 = child(node, 2);
    struct ast *child3 = child(node, 3);
    struct attr *a, *b;
    switch (child1->type) {
        case Exp:
            a = SEM_Exp(child1);
            if (a == NULL)
                return NULL;
            if (child2->type != LB && child2->type != DOT) {
                b = SEM_Exp(child3);
                if (child2->type == ASSIGNOP) {
                    if ((a->kind == BASIC && a->basic->name == NULL) || a->kind == FUNCTION) {
                        SEMERROR_NOVA(AssignLeftValue, child1);
                        return NULL;
                    } else if (b && !isequal(a, b)) { //error not match
//                    printf("error type not match\n");
                        SEMERROR_NOVA(AssignMismatch, child2);
                        return NULL;
                    }
                } else { //DONETODO: other types
                    if (b && !isequal(a, b)) { //error not match
//                    printf("error type not match\n");
                        SEMERROR_NOVA(OperandsMismatch, child2);
                        return NULL;
                    }
                }
                return a;
            }
            else if (child2->type == LB) {
                if (a->kind != ARRAY) { //DONETODO: error expected ARRAY
//                    printf("error expected ARRAY\n");
                    SEMERROR_NOVA(NotArray, child1);
                    return NULL;
                }
                b = SEM_Exp(child3);
                if (b->kind != BASIC || b->basic->type != INT_T) { //DONETODO: error expected int
//                    printf("error expected int\n");
                    char buf[24];
                    if (child(child3, 1)->type == FLOAT) {
                        sprintf(buf, "\"%lg\"", child(child3, 1)->val->fpval);
                    } else
                        strcpy(buf, "Array argument");
                    SEMERROR(NotInteger, child3, buf);
                    return NULL;
                }
                //UNKNOWNTODO: compose array type, REALLY NEED?
                return a->array->type;
            } else { // access struct field
                if (a->kind != STRUCTURE) { //DONETODO: error expected struct
//                    printf("error expected struct\n");
                    SEMERROR_NOVA(NotStruct, child2);
                    return NULL;
                }
                //DONETODO: search each field for ID
                struct field_t *structField = a->structure->field;
                for (; structField; structField = structField->next) {
                    if (strcmp(structField->name, child3->val->idval) == 0) {
                        break;
                    }
                }
                if (structField) {
                    return structField->type;
                } else {
                    SEMERROR(StructHasNoField, child3, child3->val->idval);
                    return NULL;
                }
            }
            break;
        case NOT:
        case MINUS:
            return SEM_Exp(child2);
        case ID:
            a = findvar(child1->val->idval);
            if (!child3) {
                if (a == NULL) { //DONETODO: error not found
                    //printf("error var not found: %s\n", child1->val->idval);
                    SEMERROR(UndefinedVariable, child1, child1->val->idval);
                }
                return a;
            }
            if (a) {
                SEMERROR(NotFunction, child1, child1->val->idval);
                return NULL;
            }
            a = findfunc(child1->val->idval);
            if (a == NULL) { //DONETODO: error func not decl
                //printf("error func not found: %s\n", child1->val->idval);
                SEMERROR(UndefinedFunction, child1, child1->val->idval);
                return a;
            }
            char *s1, *s2;
            if (!isFuncMatch(child1, a, &s1, &s2)) {
                //DONETODO check arg type
                SEMERROR(FunctionArgMismatch, child1, s1, s2);
                free(s1);
                free(s2);
//                return NULL;
            }
            return a;
        case INT:
        case FLOAT:
            a = malloc(sizeof(struct attr));
            a->kind = BASIC;
            a->basic = malloc(sizeof(struct basic_t));
            a->basic->type = child1->type == INT ? INT_T : FLOAT_T;
            a->basic->name = NULL;
            return a;
        case LP:
            return SEM_Exp(child2);
        default:
            break;
    }
    abort();
}

static void SEM_RETURN(struct ast *node) {
    struct attr *fun = findfunc(cur_func);
    if (fun)
        fun = fun->function->return_type;
    struct attr *b = SEM_Exp(node);
    if (b && !isequal(fun, b)) { //DONETODO: error not match
//        printf("error return type\n");
        SEMERROR_NOVA(ReturnMismatch, node);
    }
}

static void SEM_IF(struct ast *node) {
    struct attr *expr_type = SEM_Exp(sibling(node, 2));
    if (expr_type == NULL)
        return;
    while (expr_type->kind == ARRAY) {
        expr_type = expr_type->array->type;
    }
    if (expr_type->kind == FUNCTION) {
        expr_type = expr_type->function->return_type;
    }
    if (expr_type->kind != BASIC || expr_type->basic->type != INT_T) { //DONETODO: check expr_type
//        printf("error if/while condition type\n");
        SEMERROR(NotInteger, node, "#IF/WHILE CONDITION#");
    }
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
#if COMPILER_VERSION < 3
    newenv();
#endif
    for (; f; f = f->next) {
        struct attr *a = f->type;
        if (a->kind == STRUCTURE) {
            a = findvar(a->structure->name);
            if (a == NULL) {
                SEMERROR(UndefinedStruct, node, a->structure->name);
            } else
                insertvar(f->name, a);
        } else
            insertvar(f->name, f->type);
    }
    SEM_DefList(child(node, 2), 1);
    SEM_StmtList(child(node ,3));
#if COMPILER_VERSION < 3
    deleteenv();
#endif
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
            SEM_Ext_SEMI(spec_type, child(node, 1));
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
#if COMPILER_VERSION >= 3
    struct attr readFunc, writeFunc;
    readFunc.kind = writeFunc.kind = FUNCTION;
    readFunc.function = malloc(sizeof(struct func_t));
    writeFunc.function = malloc(sizeof(struct func_t));
    memset(readFunc.function, 0, sizeof(struct func_t));
    memset(writeFunc.function, 0, sizeof(struct func_t));

    readFunc.function->return_type = malloc(sizeof(struct attr));
    writeFunc.function->return_type = malloc(sizeof(struct attr));
    memset(readFunc.function->return_type, 0, sizeof(struct attr));
    memset(writeFunc.function->return_type, 0, sizeof(struct attr));

    readFunc.function->name = "read";
    writeFunc.function->name = "write";

    readFunc.function->arg_list = NULL;
    writeFunc.function->arg_list = malloc(sizeof(struct arg_t));
    memset(writeFunc.function->arg_list, 0, sizeof(struct arg_t));

    readFunc.function->return_type->kind = BASIC;
    writeFunc.function->return_type->kind = BASIC;

    readFunc.function->return_type->basic = malloc(sizeof(struct basic_t));
    writeFunc.function->return_type->basic = malloc(sizeof(struct basic_t));
    memset(readFunc.function->return_type->basic, 0, sizeof(struct basic_t));
    memset(writeFunc.function->return_type->basic, 0, sizeof(struct basic_t));

    readFunc.function->return_type->basic->type = INT_T;
    writeFunc.function->return_type->basic->type = INT_T;

    writeFunc.function->arg_list->arg = writeFunc.function->return_type;

    insertfunc(readFunc.function->name, &readFunc);
    insertfunc(writeFunc.function->name, &writeFunc);

#endif
    SEM_ExtDefList(child(node, 1));

#if COMPILER_VERSION < 3
    deleteallenv();
#endif
}

const char *semErrorMsg[18] = {
        "Other errors: %s",
        "Undefined variable \"%s\".",
        "Undefined function \"%s\".",
        "Redefined variable \"%s\".",
        "Redefined function \"%s\".",
        "Type mismatched for assignment.",
        "The left-hand side of an assignment must be a variable.",
        "Type mismatched for operands.",
        "Type mismatched for return.",
        "Function \"%s\" is not applicable for arguments \"%s\".",
        "Illegal use of \"[]\".",
        "\"%s\" is not a function.",
        "%s is not an integer.",
        "Illegal use of \".\".",
        "Non-existent field \"%s\".",
        "Redefined field \"%s\".",
        "Duplicated name \"%s\".",
        "Undefined structure \"%s\".",
};

void semchecker() {
    SEM_Program(ast_root);
}

#else

typedef int make_iso_compilers_happy;

#endif
