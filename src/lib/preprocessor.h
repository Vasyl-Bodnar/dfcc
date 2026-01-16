#ifndef PREPROCESSOR_H_
#define PREPROCESSOR_H_

#include "lexer.h"

typedef struct MacroDefine {
    size_t id;
    Span span; // what to replace with
    Ids *args;
} MacroDefine;

typedef Vector Macros;

Lex preprocessed_lex_next(const char *input, size_t len, size_t *idx,
                          size_t *if_depth, Ids **id_table,
                          Macros **macro_table);

#endif // PREPROCESSOR_H_
