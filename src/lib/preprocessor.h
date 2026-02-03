#ifndef PREPROCESSOR_H_
#define PREPROCESSOR_H_

#include "got.h"
#include "lexer.h"
#include <stdio.h>

typedef struct MacroDefine {
    Ids *args;
    Span span; // what to replace with
} MacroDefine;

enum include_type {
    IncludeMacro,
    IncludeFile,
};

// Note that `IncludeMacro` does not own anything
// while `IncludeFile` owns the path and input
typedef struct IncludeResource {
    enum include_type incl_type;
    union {
        String *path;
        size_t macro_id;
    };
    char *input;
    size_t len;
    size_t idx;
} IncludeResource;

typedef Vector Includes;

typedef Vector IncludeStack; // ids to Includes

typedef HashTable Macros;

Lex preprocessed_lex_next(IncludeStack **incl_stack, size_t *if_depth,
                          Ids **id_table, Includes **includes,
                          Macros **macro_table);

IncludeStack *create_include_stack(size_t capacity);
Includes *create_includes(size_t capacity);
Macros *create_macros(size_t capacity);

size_t insert_include(Includes **includes, IncludeResource *resource);

IncludeResource *get_top_include(IncludeStack *incl_stack, Includes *includes);
void pop_top_include(IncludeStack *incl_stack, Includes *includes,
                     Macros *macros);

void print_include_stack(IncludeStack *include_stack);
void print_includes(Includes *includes);
void print_macros(Macros *macro_table);

void free_include_stack(IncludeStack *incl_stack);
void free_includes(Includes *includes);
void free_macros(Macros *macro_table);

#endif // PREPROCESSOR_H_
