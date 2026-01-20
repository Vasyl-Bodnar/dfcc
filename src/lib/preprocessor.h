#ifndef PREPROCESSOR_H_
#define PREPROCESSOR_H_

#include "got.h"
#include "lexer.h"

typedef struct MacroDefine {
    Ids *args;
    Span span; // what to replace with
} MacroDefine;

typedef HashTable Macros;

Lex preprocessed_lex_next(const char *input, size_t len, size_t *idx,
                          size_t *if_depth, Ids **id_table,
                          Macros **macro_table);

Macros *create_macros(size_t capacity);

#endif // PREPROCESSOR_H_
