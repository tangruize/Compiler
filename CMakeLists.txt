cmake_minimum_required(VERSION 3.6)
project(Compiler)
include_directories(include ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -Wpedantic -Werror -Wno-unused-function")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-g -Wall -Wextra -Wpedantic -Werror -Wno-unused-function")

find_package(FLEX)
find_package(BISON)
FLEX_TARGET(Scanner lex/lexer.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.c COMPILE_FLAGS --header-file=${CMAKE_CURRENT_BINARY_DIR}/lexer.h)
BISON_TARGET(Parser parser/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c COMPILE_FLAGS --defines=${CMAKE_CURRENT_BINARY_DIR}/parser.h)
ADD_FLEX_BISON_DEPENDENCY(Scanner Parser)

set(SOURCE_FILES src/main.c src/error.c src/tree.c)
add_executable(Compiler ${SOURCE_FILES} ${BISON_Parser_OUTPUTS} ${FLEX_Scanner_OUTPUTS})
target_link_libraries(Compiler ${FLEX_LIBRARIES} ${BISON_LIBRARIES})