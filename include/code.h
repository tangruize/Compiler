//
// Created by tangruize on 18-1-9.
//

#ifndef COMPILER_CODE_H
#define COMPILER_CODE_H

#include "version.h"

#if COMPILER_VERSION >= 4

#ifdef __cplusplus
extern "C" {
#endif

void genCode();

#ifdef __cplusplus
}
#endif

#endif //COMPILER_VERSION >= 4

#endif //COMPILER_CODE_H
