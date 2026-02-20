#ifndef PP_H
#define PP_H

#include "got.h"
#include "lexer.h"

typedef struct DefineMacro {
    Ids *args;
    Lexes *lexes; // what to replace with
} DefineMacro;

enum include_type {
    IncludeMacro,
    IncludeFile,
};

// `path` and `input.start` must be freed for IncludeFile.
// `lexes` must be freed for IncludeMacro.
typedef struct IncludeResource {
    enum include_type type;
    union {
        String *path;
        size_t mid;
    };
    union {
        Stream stream;
        struct {
            size_t idx;
            Lexes *lexes;
        };
    };
} IncludeResource;

typedef Vector Includes;     // IncludeResources
typedef Vector IncludeStack; // Ids to Includes
typedef HashTable Macros;    // MacroDefines

typedef struct Preprocessor {
    Includes *incl_table;
    IncludeStack *incl_stack;
    Macros *macro_table;
    size_t macro_if_depth;
} Preprocessor;

Lex pp_lex_next(Preprocessor *pp, Ids **id_table);

Lex include_file(Preprocessor *pp, String *path);

Preprocessor *create_pp();
void delete_pp(Preprocessor *pp);

#endif // PP_H
