//
// Created by tangruize on 17-11-25.
//

#ifndef COMPILER_TYPE_H
#define COMPILER_TYPE_H

void semchecker();

enum {
    OtherError = 0, UndefinedVariable = 1, UndefinedFunction, RedefinedVariable, RedefinedFunction,
    AssignMismatch, AssignLeftValue, OperandsMismatch, ReturnMismatch, FunctionArgMismatch,
    NotArray, NotFunction, NotInteger, NotStruct, StructHasNoField, RedefinedField,
    DuplicatedStructName, UndefinedStruct
};

const char *semErrorMsg[18];

#endif //COMPILER_TYPE_H
