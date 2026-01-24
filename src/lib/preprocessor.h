#ifndef PREPROCESSOR_H_
#define PREPROCESSOR_H_

#include "got.h"
#include "lexer.h"
#include <stdio.h>

typedef struct MacroDefine {
    Ids *args;
    Span span; // what to replace with
} MacroDefine;

typedef struct IncludeFile {
    FILE *file;
    String *path;
    char *input;
    size_t len;
    size_t idx;
} IncludeFile;

typedef Vector IncludeStack;

typedef HashTable Macros;

Lex preprocessed_lex_next(IncludeStack **incl_stack, size_t *if_depth,
                          Ids **id_table, Macros **macro_table);

IncludeStack *create_include_stack(size_t capacity);
Macros *create_macros(size_t capacity);

void print_include_stack(IncludeStack *include_stack);
void print_macros(const char *input, Macros *macro_table);

void free_include_stack(IncludeStack *incl_stack);
void free_macros(Macros *macro_table);

#endif // PREPROCESSOR_H_
