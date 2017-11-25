cmake_minimum_required(VERSION 3.6)
project(Compiler)
include_directories(include ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -Wpedantic -Werror -Wno-unused-function -Wno-unused-parameter")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -Wpedantic -Werror -Wno-unused-function -Wno-unused-parameter")

find_package(FLEX)
find_package(BISON)
flex_target(Scanner lex/lexer.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.c COMPILE_FLAGS --header-file=${CMAKE_CURRENT_BINARY_DIR}/lexer.h)
bison_target(Parser parser/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c COMPILE_FLAGS --defines=${CMAKE_CURRENT_BINARY_DIR}/parser.h)
add_flex_bison_dependency(Scanner Parser)

set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/lexer.h")

set(SOURCE_FILES lex/lexer.l parser/parser.y src/main.c src/error.c include/error.h src/tree.c include/tree.h src/semantics.c include/semantics.h src/symbol.cpp include/symbol.h)
add_executable(Compiler ${SOURCE_FILES} ${BISON_Parser_OUTPUTS} ${FLEX_Scanner_OUTPUTS})
target_link_libraries(Compiler ${FLEX_LIBRARIES} ${BISON_LIBRARIES})