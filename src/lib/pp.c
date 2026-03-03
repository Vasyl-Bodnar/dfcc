/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "pp.h"
#include "got.h"
#include "lexer.h"
#include "vec.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

Lex lex_next_top_expand(Preprocessor *pp);
IncludeResource *get_top_resc(Preprocessor *pp);
Lex include_macro(Preprocessor *pp, IncludeResource partial);

Lex macro_include_file(Preprocessor *pp) {
    IncludeResource *top = get_top_resc(pp);
    if (top->type != IncludeFile) {
        return (Lex){.type = LEX_Invalid, .invalid = ExpectedFileNotMacro};
    }

    Lex lex = lex_next(&top->stream, &pp->id_table);
    if (lex.type == LEX_Left) {
        Span str = {.start = lex.span.start + 1, .len = 0};
        while (str.start[str.len] != '>') {
            if (str.start[str.len] == '\n') {
                return (Lex){.type = LEX_Invalid,
                             .span = str,
                             .invalid = ExpectedValidIncludeFile};
            }
            str.len += 1;
            top->stream.idx += 1;
            top->stream.col += 1;
        }
        top->stream.idx += 1;
        top->stream.col += 1;

        if (!str.len) {
            return (Lex){.type = LEX_Invalid,
                         .span = str,
                         .invalid = ExpectedValidIncludeFile};
        }

        char path[PATH_MAX];

        // TODO: We actually need to check system path like /usr/local, etc.
        // Also the pathes included with -Ipath
        realpath(top->path->s, path);
        size_t path_len = strlen(path);

        for (; path[path_len] != '/'; path_len--)
            ;

        memcpy(path + path_len + 1, str.start, str.len);
        path[path_len + 1 + str.len] = '\0';

        String *file_path = from_cstr(path);

        if (include_file(pp, file_path)) {
            return (Lex){.type = LEX_Invalid,
                         .invalid = ExpectedValidIncludeFile};
        } else {
            return pp_lex_next(pp);
        }
    } else if (lex.type == LEX_String) {
        char path[PATH_MAX];
        Span str = *(Span *)at_elem_vec(pp->id_table, lex.id);

        realpath(top->path->s, path);
        size_t path_len = strlen(path);

        for (; path[path_len] != '/'; path_len--)
            ;

        memcpy(path + path_len + 1, str.start + 1, str.len - 2);
        path[path_len + str.len - 1] = '\0';

        String *file_path = from_cstr(path);

        if (include_file(pp, file_path)) {
            return (Lex){.type = LEX_Invalid,
                         .invalid = ExpectedValidIncludeFile};
        } else {
            return pp_lex_next(pp);
        }
    } else {
        return (Lex){.type = LEX_Invalid,
                     .span = lex.span,
                     .invalid = ExpectedIncludeHeader};
    }
}

Lex lex_next_top(Preprocessor *pp) {
    if (pp->incl_stack->length) {
        IncludeResource *resc = get_top_resc(pp);
        Lex lex;
        if (resc->type == IncludeFile) {
            lex = lex_next(&resc->stream, &pp->id_table);

            if (lex.type == LEX_Eof) {
                resc->stream.idx = 0;
                resc->stream.col = 0;
                resc->stream.row = 0;
                resc->stream.macro_line = 0;
                pop_elem_vec(pp->incl_stack);
                return lex_next_top(pp);
            }

            return lex;
        } else {
            if (resc->idx >= resc->lexes->length) {
                resc->idx = 0;
                resc->macro_line = 0;
                pop_elem_vec(pp->incl_stack);
                return lex_next_top(pp);
            } else {
                return *(Lex *)at_elem_vec(resc->lexes, resc->idx++);
            }
        }
    }
    return (Lex){.type = LEX_Eof};
}

// NOTE: A bit of IncludeResource abuse below
IncludeResource scan_macros(Preprocessor *pp, size_t id) {
    for (size_t i = pp->incl_stack->length - 1; i != (size_t)-1; --i) {
        size_t idx = *(size_t *)at_elem_vec(pp->incl_stack, i);
        IncludeResource *resc = at_elem_vec(pp->incl_table, idx);
        if (resc->type == IncludeMacro) {
            DefineMacro *macro = get_elem_dht(pp->macro_table, &resc->mid);
            if (macro && macro->args) {
                for (size_t j = 0; j < macro->args->length; j++) {
                    size_t idx = *(size_t *)at_elem_vec(macro->args, j);
                    if (idx == id) {
                        Lexes **lexes = at_elem_vec(resc->args, j);
                        return (IncludeResource){.type = IncludeParameter,
                                                 .idx = 0,
                                                 .lexes = *lexes,
                                                 .args = 0,
                                                 .mid = id,
                                                 .macro_line = 0};
                    }
                }
            }
        }
    }

    DefineMacro *macro = get_elem_dht(pp->macro_table, &id);
    if (macro) {
        Args *args = 0;
        if (macro->args) {
            args = create_args(macro->args->length);
        }
        return (IncludeResource){.type = IncludeMacro,
                                 .idx = 0,
                                 .lexes = macro->lexes,
                                 .args = args,
                                 .mid = id,
                                 .macro_line = 0};
    } else {
        return (IncludeResource){
            .type = InvalidInclude,
        };
    }
}

Lex lex_next_top_expand(Preprocessor *pp) {
    Lex lex = lex_next_top(pp);
    if (lex.type == LEX_Identifier) {
        IncludeResource partial = scan_macros(pp, lex.id);
        if (partial.type != InvalidInclude) {
            return include_macro(pp, partial);
        }
    }
    return lex;
}

int top_macro_line(Preprocessor *pp) {
    if (pp->incl_table->length) {
        IncludeResource *resc = get_top_resc(pp);
        if (resc->type == IncludeFile) {
            return resc->stream.macro_line;
        } else {
            return resc->macro_line;
        }
    }
    return 0;
}

// TODO: Variadic
// Potentially handle arguments in the lexer
Lex define_macro(Preprocessor *pp) {
    Lex lex = lex_next_top(pp);
    if (lex.type != LEX_Identifier) {
        return (Lex){.type = LEX_Invalid,
                     .span = lex.span,
                     .invalid = ExpectedIdMacroDefine};
    }

    size_t mid = lex.id;
    Lexes *lexes = create_lexes(0);
    Ids *args = 0;

    lex = lex_next_top_expand(pp);
    // TODO: This must be next to id, no space allowed
    if (lex.type == LEX_LParen) {
        args = create_idsref(1);

        Lex prev;
        while (lex.type != LEX_RParen) {
            prev = lex;
            lex = lex_next_top(pp);
            if (lex.type == LEX_Comma || lex.type == LEX_RParen) {
            } else if ((prev.type == LEX_LParen || prev.type == LEX_Comma) &&
                       lex.type == LEX_Identifier) {
                push_elem_vec(&args, &lex.id);
            } else {
                delete_vec(lexes);
                delete_vec(args);
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedGoodArgsMacroDefine};
            }
        }

        lex = lex_next_top_expand(pp);
    }

    while (1) {
        if (lex.type == LEX_MacroEndToken) {
            break;
        }
        push_elem_vec(&lexes, &lex);
        lex = lex_next_top_expand(pp);
    }

    put_elem_dht(&pp->macro_table, &mid,
                 &(DefineMacro){.args = args, .lexes = lexes});

    return pp_lex_next(pp);
}

// TODO: We do not need to actually lex the stream
// Requires augments to the lexer
// `else_clause` is for when we took a branch already and just need to endif
Lex skip_if_clause(Preprocessor *pp, size_t depth, int else_clause) {
    Lex lex;
    do {
        lex = lex_next_top(pp);
    } while (lex.type != LEX_MacroToken);

    if (else_clause) {
        do {
            lex = lex_next_top(pp);
            if (lex.type == LEX_Eof) {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIfEndIf};
            }
        } while (lex.type != LEX_MacroToken && lex.macro != EndIf);

        return pp_lex_next(pp);
    }

    // NOTE: We can only be here if we did not take the previous branches
    switch (lex.macro) {
    case Else:
        return pp_lex_next(pp);
    // TODO: case ElseIf:
    case ElseIfDefined:
        lex = lex_next_top(pp);
        if (lex.type == LEX_Identifier) {
            DefineMacro *macro = get_elem_dht(pp->macro_table, &lex.id);
            if (macro) {
                return pp_lex_next(pp);
            } else {
                return skip_if_clause(pp, pp->macro_if_depth, else_clause);
            }
        } else {
            return (Lex){.type = LEX_Invalid,
                         .span = lex.span,
                         .invalid = ExpectedIdIfDef};
        }
    case ElseIfNotDefined:
        lex = lex_next_top(pp);
        if (lex.type == LEX_Identifier) {
            DefineMacro *macro = get_elem_dht(pp->macro_table, &lex.id);
            if (macro) {
                return skip_if_clause(pp, pp->macro_if_depth, else_clause);
            } else {
                return pp_lex_next(pp);
            }
        } else {
            return (Lex){.type = LEX_Invalid,
                         .span = lex.span,
                         .invalid = ExpectedIdIfDef};
        }
    case EndIf:
        pp->macro_if_depth -= 1;
        return pp_lex_next(pp);
    default:
        return skip_if_clause(pp, depth, else_clause);
    }
}

Lex pp_lex_next(Preprocessor *pp) {
    Lex lex = lex_next_top_expand(pp);

    if (lex.type == LEX_MacroToken) {
        switch (lex.macro) {
        case InvalidMacro:
            return (Lex){.type = LEX_Invalid,
                         .span = lex.span,
                         .invalid = ExpectedValidMacro};
        case Include:
            return macro_include_file(pp);
        case Error:
            lex = lex_next_top_expand(pp);
            if (lex.type == LEX_String) {
                printf("#error %.*s on line %zu\n", (int)lex.span.len,
                       lex.span.start, lex.span.row);
                return (Lex){.type = LEX_Eof};
            } else {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedStringErrorMacro};
            }
        case Warning:
            lex = lex_next_top_expand(pp);
            if (lex.type == LEX_String) {
                printf("#warning %.*s on line %zu\n", (int)lex.span.len,
                       lex.span.start, lex.span.row);
                return pp_lex_next(pp);
            } else {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedStringWarnMacro};
            }
        case Define:
            return define_macro(pp);
        case Undefine:
            lex = lex_next_top(pp);
            if (lex.type == LEX_Identifier) {
                deletecb_elem_dht(pp->macro_table, &lex.id, clean_macro);
                return pp_lex_next(pp);
            } else {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIdMacroUndefine};
            }
        case If:
            pp->macro_if_depth += 1;
            // TODO:
            return lex;
        case IfDefined:
            lex = lex_next_top(pp);
            if (lex.type == LEX_Identifier) {
                DefineMacro *macro = get_elem_dht(pp->macro_table, &lex.id);
                pp->macro_if_depth += 1;
                if (macro) {
                    return pp_lex_next(pp);
                } else {
                    return skip_if_clause(pp, pp->macro_if_depth, 0);
                }
            } else {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIdIfDef};
            }
        case IfNotDefined:
            lex = lex_next_top(pp);
            if (lex.type == LEX_Identifier) {
                DefineMacro *macro = get_elem_dht(pp->macro_table, &lex.id);
                pp->macro_if_depth += 1;
                if (macro) {
                    return skip_if_clause(pp, pp->macro_if_depth, 0);
                } else {
                    return pp_lex_next(pp);
                }
            } else {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIdIfDef};
            }
        case Else:
        case ElseIf:
        case ElseIfDefined:
        case ElseIfNotDefined:
            // NOTE: We can only be here if we took the previous branch
            return skip_if_clause(pp, pp->macro_if_depth, 1);
        case EndIf:
            pp->macro_if_depth -= 1;
            return pp_lex_next(pp);
        case Line:
            // TODO:
            return lex;
        case Embed:
            // TODO:
            return lex;
        case Pragma:
            // TODO:
            return lex;
        }
    }

    return lex;
}

void insert_include_macro(Preprocessor *pp, IncludeResource *macro_resc) {
    for (size_t i = 0; i < pp->incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(pp->incl_table, i);
        if ((resc->type == IncludeMacro || resc->type == IncludeParameter) &&
            !resc->idx && resc->mid == macro_resc->mid &&
            resc->lexes == macro_resc->lexes) {
            resc->args = macro_resc->args;
            push_elem_vec(&pp->incl_stack, &i);
            return;
        }
    }
    size_t id = pp->incl_table->length;
    push_elem_vec(&pp->incl_table, macro_resc);
    push_elem_vec(&pp->incl_stack, &id);
}

Lex include_macro(Preprocessor *pp, IncludeResource partial) {
    if (partial.args) {
        Lexes *lexes = create_lexes(0);
        Lex lex = lex_next_top_expand(pp);
        if (lex.type == LEX_LParen) {
            size_t depth = 1;
            while (depth) {
                lex = lex_next_top_expand(pp);
                if (lex.type == LEX_Comma) {
                    push_elem_vec(&partial.args, &lexes);
                    lexes = create_lexes(0);
                } else if (lex.type == LEX_RParen) {
                    depth -= 1;
                } else if (lex.type == LEX_LParen) {
                    depth += 1;
                } else {
                    push_elem_vec(&lexes, &lex);
                }
            }
            push_elem_vec(&partial.args, &lexes);
        } else {
            delete_vec(lexes);
            delete_args(partial.args);
            return (Lex){.type = LEX_Invalid,
                         .span = lex.span,
                         .invalid = ExpectedValidMacro};
        }
    }

    insert_include_macro(pp, &partial);

    // Slightly reworked lex_next_top_expand to avoid infinite recursion bug
    Lex lex = lex_next_top(pp);
    if (lex.type == LEX_Identifier && lex.id != partial.mid) {
        IncludeResource another = scan_macros(pp, lex.id);
        if (another.type != InvalidInclude) {
            return include_macro(pp, another);
        }
    }
    return lex;
}

int include_file(Preprocessor *pp, String *path) {
    // If already present and unused, we don't need to allocate again
    for (size_t i = 0; i < pp->incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(pp->incl_table, i);
        if (resc->type == IncludeFile && !resc->stream.idx &&
            resc->path->length == path->length &&
            !memcmp(resc->path->s, path->s, resc->path->length)) {
            push_elem_vec(&pp->incl_stack, &i);
            return 0;
        }
    }

    FILE *file = fopen(path->s, "r");
    if (!file) {
        printf("File \"%.*s\" could not be found\n", (int)path->length,
               path->s);
        return 1;
    }

    struct stat st_buf;
    fstat(fileno(file), &st_buf);

    char *buf = malloc(st_buf.st_size + 1);
    size_t bytes = fread(buf, 1, st_buf.st_size, file);
    fclose(file);

    if (bytes == 0) {
        free(buf);
        return 1;
    }

    if (buf[bytes - 1] != '\\') {
        buf[bytes] = '\n';
    } else {
        puts("Last character cannot be '\\'");
        free(buf);
        return 1;
    }

    push_elem_vec(&pp->incl_table, &(IncludeResource){
                                       .type = IncludeFile,
                                       .path = path,
                                       .stream = {.start = buf,
                                                  .len = bytes,
                                                  .idx = 0,
                                                  .row = 1,
                                                  .col = 0,
                                                  .macro_line = 0},
                                   });

    size_t id = pp->incl_table->length - 1;
    push_elem_vec(&pp->incl_stack, &id);

    return 0;
}

// NOTE: Careful when relying on top.
// The top resource can easily change due to include_file or include_macro.
IncludeResource *get_top_resc(Preprocessor *pp) {
    size_t idx =
        *(size_t *)at_elem_vec(pp->incl_stack, pp->incl_stack->length - 1);
    return at_elem_vec(pp->incl_table, idx);
}

void print_macro_table(Macros *macro_table) {
    Entry entry;
    size_t idx = 0;
    while ((entry = next_elem_dht(macro_table, &idx)).key) {
        DefineMacro *macro = entry.value;
        printf("#string-id: %zu", *(size_t *)entry.key);
        if (macro->args) {
            printf("\n.args-len: %zu args:", macro->args->length);
            for (size_t i = 0; i < macro->args->length; i++) {
                printf("\nid: %zu", *(size_t *)at_elem_vec(macro->args, i));
            }
            if (macro->lexes) {
                printf("\n.elems-len: %zu elems:\n", macro->lexes->length);
                print_lexes(macro->lexes);
            }
            printf("#\n");
            continue;
        }
        if (macro->lexes) {
            if (macro->lexes->length) {
                printf(" elems-len: %zu elems:\n", macro->lexes->length);
                print_lexes(macro->lexes);
            } else {
                printf(" elems-len: %zu elems: none", macro->lexes->length);
            }
        }
        printf("#\n");
    }
}

void print_incl_table(Includes *incl_table) {
    for (size_t i = 0; i < incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(incl_table, i);
        if (resc->type == IncludeFile) {
            printf(
                "<FILE: path: %s base-ptr: %p len: %zu row: %zu col: %zu idx: "
                "%zu>\n",
                resc->path->s, resc->stream.start, resc->stream.len,
                resc->stream.row, resc->stream.col, resc->stream.idx);
        } else if (resc->type == IncludeMacro) {
            printf("<MACRO: mid: %zu token-idx: %zu", resc->mid, resc->idx);
            if (resc->args) {
                printf(" args-len: %zu args: %p", resc->args->length,
                       resc->args);
            }
            if (resc->lexes) {
                printf(" elems-len: %zu elems:\n", resc->lexes->length);
                print_lexes(resc->lexes);
            }
            printf(">\n");
        } else if (resc->type == IncludeParameter) {
            printf("<PARAM: mid: %zu token-idx: %zu", resc->mid, resc->idx);
            if (resc->lexes) {
                printf(" elems-len: %zu elems:\n", resc->lexes->length);
                print_lexes(resc->lexes);
            }
            printf(">\n");
        } else {
            printf("<INVALID RESOURCE>\n");
        }
    }
}

void print_incl_stack(IncludeStack *incl_stack) {
    for (size_t i = 0; i < incl_stack->length; i++) {
        size_t *id = at_elem_vec(incl_stack, i);
        printf("(include-idx: %zu)\n", *id);
    }
}

Args *create_args(size_t capacity) {
    return create_vec(capacity, sizeof(Lexes *));
}

void delete_args(Args *args) {
    for (size_t j = 0; j < args->length; j++) {
        delete_vec(*(Lexes **)at_elem_vec(args, j));
    }
    delete_vec(args);
}

IdsRef *create_idsref(size_t capacity) {
    return create_vec(capacity, sizeof(size_t));
}

// Free DefineMacro's members
void clean_macro(void *define_macro) {
    DefineMacro *macro = define_macro;
    if (macro->args)
        free(macro->args);
    if (macro->lexes)
        free(macro->lexes);
}

Preprocessor *create_pp() {
    Preprocessor *pp = malloc(sizeof(*pp));
    pp->incl_table = create_vec(8, sizeof(IncludeResource));
    pp->incl_stack = create_vec(8, sizeof(size_t));
    pp->macro_table = create_dht(8, sizeof(size_t), sizeof(DefineMacro));
    pp->id_table = create_ids(8);
    pp->macro_if_depth = 0;
    return pp;
}

void print_pp(Preprocessor *pp) {
    printf("(PREPROCESSOR: macro-if-depth: %zu\n", pp->macro_if_depth);
    printf("include-stack:\n");
    print_incl_stack(pp->incl_stack);
    printf("include-table:\n");
    print_incl_table(pp->incl_table);
    printf("defined-macros:\n");
    print_macro_table(pp->macro_table);
    printf("id-table:\n");
    print_ids(pp->id_table);
    printf(")");
}

// TODO: This might not free properly
void delete_pp(Preprocessor *pp) {
    for (size_t i = 0; i < pp->incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(pp->incl_table, i);
        if (resc->type == IncludeFile) {
            delete_str(resc->path);
            free(resc->stream.start);
        } else if (resc->type == IncludeMacro) {
            if (resc->args) {
                delete_args(resc->args);
            }
        }
    }
    delete_vec(pp->incl_table);
    delete_vec(pp->incl_stack);
    delete_vec(pp->id_table);
    Entry entry;
    size_t idx = 0;
    while ((entry = next_elem_dht(pp->macro_table, &idx)).key) {
        clean_macro(entry.value);
    }
    delete_dht(pp->macro_table);
    free(pp);
}
