/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PP_H
#define PP_H

#include "got.h"
#include "lexer.h"

typedef HashTable Macros;    // Mid -> DefineMacro hashtable
typedef Vector Args;         // Each arg is Lexes*
typedef Vector Includes;     // Actual IncludeResources
typedef Vector IdsRef;       // Idxs to Ids
typedef Vector IncludeStack; // Idxs to Includes

/* The way macros work.
 * We have three systems here, *DefineMacro*, *IncludeResource*, *Preprocessor*.
 * Preprocessor contains all the macro definitions in a hashtable.
 *
 * *DefineMacro* is that definition, referenced by its own id.
 * E.g. `#define x\\n` will be a *DefineMacro* at id of x with no lexes or args.
 * Args may be references to paramater ids.
 * Lexes may be the actual lexes to replace with.
 *
 * *IncludeResource* is the boots on the ground, what is currently being
 * replaced and exhausted. It uses a text stream or a macro.
 *
 * The macro in this case has its id, lexes, and lexes of arguments (to replace
 * paramaters), as well as idx on where it currently is in the main lexes.
 *
 * *Preprocessor* then utilizes *IncludeResources* on a stack to produce text or
 * lexes directly.
 *
 * *DefineMacro* thus acts as a blueprint from an id to create a real
 * exhaustable *IncludeResource* for use of the *Preprocessor* to produce
 * the next lex.
 */
typedef struct DefineMacro {
    IdsRef *args;
    Lexes *lexes; // what to replace with
} DefineMacro;

enum include_type {
    InvalidInclude = 0,
    IncludeMacro,
    // IncludeMacro that cannot have args and is not hashtable backed
    IncludeParameter,
    IncludeFile,
};

// `path` and `stream.start` must be freed for IncludeFile.
// `lexes` must be freed for IncludeMacro.
typedef struct IncludeResource {
    enum include_type type;
    union {
        struct {
            String *path;
            Stream stream;
        };
        struct {
            Args *args;
            Lexes *lexes;
            size_t mid;
            size_t idx;
            // see enum macro_type, where InvalidMacro is eqv to no macro
            int macro_line;
        };
    };
} IncludeResource;

typedef struct Preprocessor {
    Includes *incl_table;
    IncludeStack *incl_stack;
    Macros *macro_table;
    Ids *id_table;
    size_t macro_if_depth;
    int disabled_if; // Inside non-taken branch
} Preprocessor;

Lex pp_lex_next(Preprocessor *pp);

int include_file(Preprocessor *pp, String *path);

Preprocessor *create_pp();
void print_pp(Preprocessor *pp);
void delete_pp(Preprocessor *pp);

IdsRef *create_idsref(size_t capacity);
Args *create_args(size_t capacity);
void delete_args(Args *args);

void clean_macro(void *define_macro);

#endif // PP_H
