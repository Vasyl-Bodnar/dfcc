#include "preprocessor.h"
#include "got.h"
#include "lexer.h"
#include "limits.h"
#include "vec.h"

/*
  // Legal macro expr lexes
  enum macro_lex_type {
    MacroConstant,
    MacroIdentifier,
    MacroDefinedIdentifier,
    MacroEtEt,
    MacroPipePipe,
    MacroExclamation,
    MacroEqualEqual,
    MacroExclamationEqual,
    MacroLeft,
    MacroLeftEqual,
    MacroRight,
    MacroRightEqual,
  };
*/

Vector *process_define_args(const char *input, size_t len, size_t *idx,
                            Ids **id_table) {
    Vector *args = create_vec(0, sizeof(size_t));
    Lex cur = lex_next(input, len, idx, id_table);
    while (cur.type == LEX_Identifier) {
        push_elem_vec(&args, &cur.id);
        cur = lex_next(input, len, idx, id_table);
        if (cur.type == LEX_Comma) {
            cur = lex_next(input, len, idx, id_table);
        } else if (cur.type == LEX_RParen || cur.type == LEX_Eof) {
            return args;
        }
    }

    return 0;
}

// TODO: Handle variadic ...
Lex process_define(const char *input, size_t len, size_t *idx, Ids **id_table,
                   Macros **macro_table) {
    MacroDefine macro = (MacroDefine){0};
    Lex result = (Lex){0};

    Lex id = lex_next(input, len, idx, id_table);
    if (id.type == LEX_Identifier) {
        size_t mid = id.id;

        if (input[*idx] == '(') {
            *idx += 1;
            macro.args = process_define_args(input, len, idx, id_table);
            if (!macro.args) {
                return (Lex){.type = LEX_Invalid,
                             .span = id.span,
                             .invalid = ExpectedGoodArgsMacroDefine};
            }
        }

        Span span = {*idx, 1};
        while (input[span.start + span.len] != '\n') {
            span.len += 1;
            if (input[span.start + span.len] == '\\') {
                if (input[span.start + span.len + 1] == '\n') {
                    span.len += 2;
                } else {
                    return (Lex){.type = LEX_Invalid,
                                 .span = id.span,
                                 .invalid = ExpectedNLAfterSlashMacroDefine};
                }
            } else if (input[span.start + span.len] == '\0') {
                result = (Lex){.type = LEX_Eof};
            }
        }
        macro.span = span;

        if (!put_elem_dht(macro_table, &mid, &macro)) {
            return (Lex){
                .type = LEX_Invalid, .span = id.span, .invalid = FailedRealloc};
        }

        *idx += span.len;

        return result;
    }

    return (Lex){
        .type = LEX_Invalid, .span = id.span, .invalid = ExpectedIdMacroDefine};
}

Lex if_defined(const char *input, size_t len, size_t *idx, size_t *if_depth,
               Ids **id_table, Macros **macro_table);
Lex if_not_defined(const char *input, size_t len, size_t *idx, size_t *if_depth,
                   Ids **id_table, Macros **macro_table);

Lex conditional_exclude_tail(const char *input, size_t len, size_t *idx,
                             size_t *if_depth, Ids **id_table) {
    size_t cur_if_depth = *if_depth;
    Lex lex = (Lex){0};
    while (lex.type != LEX_Eof) {
        while ((lex = lex_next(input, len, idx, id_table)).type !=
               LEX_MacroToken)
            ;
        switch (lex.macro) {
        case If:
        case IfDefined:
        case IfNotDefined:
            *if_depth += 1;
            break;
        case EndIf:
            if (*if_depth == cur_if_depth) {
                if (!(*if_depth)) {
                    return (Lex){.type = LEX_Invalid,
                                 .span = lex.span,
                                 .invalid = ExpectedIfEndIf};
                }
                *if_depth -= 1;
                return (Lex){0};
            }
            *if_depth -= 1;
            break;
        default:
            break;
        }
    }
    return lex;
}

// Consider doing something wiser than just lexing everything needlessly
// Really we just need to jump to the next define
Lex conditional_exclude(const char *input, size_t len, size_t *idx,
                        size_t *if_depth, Ids **id_table,
                        Macros **macro_table) {
    size_t cur_if_depth = *if_depth;
    Lex lex = (Lex){0};
    while (lex.type != LEX_Eof) {
        while ((lex = lex_next(input, len, idx, id_table)).type !=
               LEX_MacroToken)
            ;
        switch (lex.macro) {
        case If:
        case IfDefined:
        case IfNotDefined:
            *if_depth += 1;
            break;
        case Else:
            if (*if_depth == cur_if_depth) {
                return (Lex){0};
            }
            break;
        case ElseIf:
            if (*if_depth == cur_if_depth) {
                // TODO: Handle
            }
            break;
        case ElseIfDefined:
            if (*if_depth == cur_if_depth) {
                return if_defined(input, len, idx, if_depth, id_table,
                                  macro_table);
            }
            break;
        case ElseIfNotDefined:
            if (*if_depth == cur_if_depth) {
                return if_not_defined(input, len, idx, if_depth, id_table,
                                      macro_table);
            }
            break;
        case EndIf:
            if (*if_depth == cur_if_depth) {
                if (!(*if_depth)) {
                    return (Lex){.type = LEX_Invalid,
                                 .span = lex.span,
                                 .invalid = ExpectedIfEndIf};
                }
                *if_depth -= 1;
                return (Lex){0};
            }
            *if_depth -= 1;
            break;
        default:
            break;
        }
    }
    return lex;
}

Lex if_defined(const char *input, size_t len, size_t *idx, size_t *if_depth,
               Ids **id_table, Macros **macro_table) {
    Lex lex = lex_next(input, len, idx, id_table);
    if (lex.type == LEX_Identifier) {
        MacroDefine *macro = get_elem_dht(*macro_table, &lex.id);
        *if_depth += 1;
        if (macro) {
            lex = (Lex){0};
        } else {
            lex = conditional_exclude(input, len, idx, if_depth, id_table,
                                      macro_table);
        }
    } else {
        return (Lex){
            .type = LEX_Invalid, .span = lex.span, .invalid = ExpectedIdIfDef};
    }

    return lex;
}

Lex if_not_defined(const char *input, size_t len, size_t *idx, size_t *if_depth,
                   Ids **id_table, Macros **macro_table) {
    Lex lex = lex_next(input, len, idx, id_table);
    if (lex.type == LEX_Identifier) {
        MacroDefine *macro = get_elem_dht(*macro_table, &lex.id);
        *if_depth += 1;
        if (macro) {
            lex = conditional_exclude(input, len, idx, if_depth, id_table,
                                      macro_table);
        } else {
            lex = (Lex){0};
        }
    } else {
        return (Lex){
            .type = LEX_Invalid, .span = lex.span, .invalid = ExpectedIdIfDef};
    }

    return lex;
}

IncludeFile *process_include(char *filename, char *include) {
    // char path[PATH_MAX];
    return 0;
}

Lex include_file(const char *input, size_t len, size_t *idx,
                 IncludeStack **incl_stack, Ids **id_table) {
    Lex lex = lex_next(input, len, idx, id_table);
    if (lex.type == LEX_Left) {
        // TODO: Parse out a filename, Add to include stack
    } else if (lex.type == LEX_String) {
        // TODO: Add to include stack
    } else {
        return (Lex){.type = LEX_Invalid,
                     .span = lex.span,
                     .invalid = ExpectedIncludeHeader};
    }
    return (Lex){0};
}

// TODO: Macros should be first characters of the line.
//       Lexer does not handle this currently
Lex preprocessed_lex_next(IncludeStack **incl_stack, size_t *if_depth,
                          Ids **id_table, Macros **macro_table) {
    IncludeFile *file = peek_elem_vec(*incl_stack);
    char *input = file->input;
    size_t len = file->len;
    size_t *idx = &file->idx;
    Lex lex = lex_next(input, len, idx, id_table);

    if (lex.type == LEX_Identifier) {
        MacroDefine *macro = get_elem_dht(*macro_table, &lex.id);
        if (macro) {
            // TODO: handle macro define expansion
        }
    } else if (lex.type == LEX_MacroToken) {
        switch (lex.macro) {
        case Define:
            lex = process_define(input, len, idx, id_table, macro_table);
            break;
        case Include:
            lex = include_file(input, len, idx, incl_stack, id_table);
            break;
        case Undefine: {
            // TODO: Find a better way
            MacroDefine *macro = get_elem_dht(*macro_table, &lex.id);
            if (macro) {
                free(macro->args); // Vector
                delete_elem_dht(*macro_table, &lex.id);
            } else {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIdMacroUndefine};
            }
            break;
        }
        case If:
            // TODO: Handle
            lex = (Lex){0};
            break;
        case IfDefined:
            lex = if_defined(input, len, idx, if_depth, id_table, macro_table);
            break;
        case IfNotDefined:
            lex = if_not_defined(input, len, idx, if_depth, id_table,
                                 macro_table);
            break;
        case Else:
            if (!(*if_depth)) {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIfElse};
            }
            lex = conditional_exclude_tail(input, len, idx, if_depth, id_table);
            break;
        case ElseIf:
            if (!(*if_depth)) {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIfElse};
            }
            // TODO: Handle
            lex = (Lex){0};
            break;
        case ElseIfDefined:
            if (!(*if_depth)) {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIfElseDef};
            }
            lex = if_defined(input, len, idx, if_depth, id_table, macro_table);
            break;
        case ElseIfNotDefined:
            if (!(*if_depth)) {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIfElseNotDef};
            }
            lex = if_not_defined(input, len, idx, if_depth, id_table,
                                 macro_table);
            break;
        case EndIf:
            if (!(*if_depth)) {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIfEndIf};
            }
            *if_depth -= 1;
            lex = (Lex){0};
            break;
        case Line:
            // TODO: Handle
            lex = (Lex){0};
            break;
        case Embed:
            // TODO: Handle
            lex = (Lex){0};
            break;
        case Error:
            lex = lex_next(input, len, idx, id_table);
            if (lex.type == LEX_String) {
                // TODO: Lines should be used
                printf("#error %.*s at char %zu\n", (int)lex.span.len,
                       input + lex.span.start, lex.span.start);
                return (Lex){.type = LEX_Eof};
            } else {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedStringErrorMacro};
            }
        case Warning:
            lex = lex_next(input, len, idx, id_table);
            if (lex.type == LEX_String) {
                // TODO: Lines should be used
                printf("#warning %.*s at char %zu\n", (int)lex.span.len,
                       input + lex.span.start, lex.span.start);
                lex = (Lex){0};
            } else {
                return (Lex){.type = LEX_Invalid,
                             .span = lex.span,
                             .invalid = ExpectedStringWarnMacro};
            }
            break;
        case Pragma:
            // TODO: Handle
            lex = (Lex){0};
            break;
        }
    }

    if (!lex.type && lex.invalid == Ok) {
        return preprocessed_lex_next(incl_stack, if_depth, id_table,
                                     macro_table);
    }

    return lex;
}

Macros *create_macros(size_t capacity) {
    return create_dht(capacity, sizeof(size_t), sizeof(MacroDefine));
}

void print_macros(const char *input, Macros *macro_table) {
    Entry entry;
    size_t idx = 0;
    while ((entry = next_elem_dht(macro_table, &idx)).key) {
        printf("(id: %zu args: %p span: [%zu, %zu])\n", *(size_t *)entry.key,
               ((MacroDefine *)entry.value)->args,
               ((MacroDefine *)entry.value)->span.start,
               ((MacroDefine *)entry.value)->span.len);
    }
}

void free_macros(Macros *macro_table) {
    Entry entry;
    size_t idx = 0;
    while ((entry = next_elem_dht(macro_table, &idx)).key) {
        free(((MacroDefine *)entry.value)->args);
    }
    free(macro_table);
}

IncludeStack *create_include_stack(size_t capacity) {
    return create_vec(capacity, sizeof(IncludeFile));
}

void print_include_stack(IncludeStack *incl_stack) {
    for (size_t i = 0; i < incl_stack->length; i++) {
        IncludeFile *file = ((IncludeFile *)incl_stack->v) + i;
        printf("(ptr: %p len: %zu idx: %zu)\n", file->input, file->len,
               file->idx);
    }
}

void free_include_stack(IncludeStack *incl_stack) {
    for (size_t i = 0; i < incl_stack->length; i++) {
        IncludeFile *file = ((IncludeFile *)incl_stack->v) + i;
        fclose(file->file);
        delete_str(file->path);
    }
    free(incl_stack);
}
