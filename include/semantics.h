//
// Created by tangruize on 17-11-25.
//

#ifndef COMPILER_TYPE_H
#define COMPILER_TYPE_H

#if COMPILER_VERSION >= 2

void semchecker();

enum {
    OtherError = 0, UndefinedVariable = 1, UndefinedFunction, RedefinedVariable, RedefinedFunction,
    AssignMismatch, AssignLeftValue, OperandsMismatch, ReturnMismatch, FunctionArgMismatch,
    NotArray, NotFunction, NotInteger, NotStruct, StructHasNoField, RedefinedField,
    DuplicatedStructName, UndefinedStruct
};

extern const char *semErrorMsg[18];

#endif

#endif //COMPILER_TYPE_H
