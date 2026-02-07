#include "preprocessor.h"
#include "got.h"
#include "lexer.h"
#include "limits.h"
#include "string.h"
#include "vec.h"
#include <sys/stat.h>

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
    MacroDefine macro = (MacroDefine){.args = 0, .span = {0}};
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

        Span span = {(char *)input + *idx, 1};
        while (span.start[span.len] != '\n') {
            span.len += 1;
            if (span.start[span.len] == '\\') {
                if (span.start[span.len + 1] == '\n') {
                    span.len += 2;
                } else {
                    return (Lex){.type = LEX_Invalid,
                                 .span = id.span,
                                 .invalid = ExpectedNLAfterSlashMacroDefine};
                }
            } else if (span.start[span.len] == '\0') {
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

// NOTE: This makes a hefty assumption that our path is always PATH_MAX at most
uint32_t process_include(const char *include, size_t len,
                         IncludeStack **incl_stack, Includes **includes) {
    IncludeResource *top = get_top_include(*incl_stack, *includes);
    char path[PATH_MAX];

    realpath(top->path->s, path);
    size_t path_len = strlen(path);

    for (; path[path_len] != '/'; path_len--)
        ;

    memcpy(path + path_len + 1, include, len);
    path[path_len + 1 + len] = '\0';

    for (size_t i = 0; i < (*includes)->length; i++) {
        IncludeResource *resc = at_elem_vec(*includes, i);
        if (resc->incl_type == IncludeFile && !resc->idx &&
            resc->path->length == path_len + 2 + len &&
            !memcmp(resc->path->s, path, resc->path->length)) {
            push_elem_vec(incl_stack, &i);
            return 1;
        }
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        return 0;
    }

    struct stat st_buf;
    fstat(fileno(file), &st_buf);

    char *buf = malloc(st_buf.st_size + 1);
    size_t bytes = fread(buf, 1, st_buf.st_size, file);
    fclose(file);

    // TODO: Better messages (and system for doing this)
    if (bytes == 0) {
        printf("Invalid include file: %s\n", path);
        free(buf);
        return 0;
    }

    if (buf[bytes - 1] != '\\') {
        buf[bytes] = '\n';
    } else {
        printf("Last character cannot be '\\' for file: %s\n", path);
        free(buf);
        return 0;
    }

    String *file_path = from_cstr(path);

    push_elem_vec(includes, &(IncludeResource){.incl_type = IncludeFile,
                                               .path = file_path,
                                               .idx = 0,
                                               .input = buf,
                                               .len = bytes});

    size_t new_top_id = (*includes)->length - 1;
    push_elem_vec(incl_stack, &new_top_id);
    return 1;
}

Lex include_file(const char *input, size_t len, size_t *idx,
                 IncludeStack **incl_stack, Includes **includes,
                 Ids **id_table) {
    Lex lex = lex_next(input, len, idx, id_table);
    if (lex.type == LEX_Left) {
        Span str = {.start = lex.span.start + 1, .len = 0};
        while (str.start[str.len] != '>') {
            if (str.start[str.len] == '\n') {
                return (Lex){.type = LEX_Invalid,
                             .span = str,
                             .invalid = ExpectedValidIncludeFile};
            }
            str.len += 1;
            *idx += 1;
        }
        *idx += 1;

        if (!str.len) {
            return (Lex){.type = LEX_Invalid,
                         .span = str,
                         .invalid = ExpectedValidIncludeFile};
        }

        if (!process_include(str.start, str.len, incl_stack, includes)) {
            return (Lex){.type = LEX_Invalid,
                         .span = str,
                         .invalid = ExpectedValidIncludeFile};
        }
    } else if (lex.type == LEX_String) {
        Span str = *(Span *)at_elem_vec(*id_table, lex.id);
        if (!process_include(str.start + 1, str.len - 2, incl_stack,
                             includes)) {
            return (Lex){.type = LEX_Invalid,
                         .span = lex.span,
                         .invalid = ExpectedValidIncludeFile};
        }
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
                          Ids **id_table, Includes **includes,
                          Macros **macro_table) {
    IncludeResource *resc = get_top_include(*incl_stack, *includes);
    char *input = resc->input;
    size_t len = resc->len;
    size_t *idx = &resc->idx;

    Lex lex = lex_next(input, len, idx, id_table);

    if (lex.type == LEX_Identifier) {
        // NOTE: Might have an id invalidation issue,
        // look into it if and when it breaks
        MacroDefine *macro = get_elem_dht(*macro_table, &lex.id);
        if (macro) {
            size_t mid = lex.id;
            if (macro->args) {
                lex = lex_next(input, len, idx, id_table);
                if (lex.type == LEX_LParen) {
                    for (size_t i = 0; i < macro->args->length; i++) {
                        size_t aid = *(size_t *)at_elem_vec(macro->args, i);
                        Span span = {.start = input + *idx, .len = 0};

                        while (span.start[span.len] != ',' &&
                               span.start[span.len] != ')') {
                            span.len += 1;
                            *idx += 1;
                            if (span.start[span.len] == '\0') {
                                return (Lex){.type = LEX_Invalid,
                                             .span = lex.span,
                                             .invalid = ExpectedMoreArgsMacro};
                            }
                        }

                        MacroDefine mac = {.args = 0, .span = span};
                        put_elem_dht(macro_table, &aid, &mac);

                        if (span.start[span.len] == ')' &&
                            i + 1 < macro->args->length) {
                            return (Lex){.type = LEX_Invalid,
                                         .span = lex.span,
                                         .invalid = ExpectedLessArgsMacro};
                        }

                        *idx += 1;
                    }

                    if (input[*idx - 1] != ')') {
                        return (Lex){.type = LEX_Invalid,
                                     .span = lex.span,
                                     .invalid = ExpectedMoreArgsMacro};
                    }
                } else {
                    return (Lex){.type = LEX_Invalid,
                                 .span = lex.span,
                                 .invalid = ExpectedMoreArgsMacro};
                }
            }

            IncludeResource resc = {
                .incl_type = IncludeMacro,
                .macro_id = mid,
                .idx = 0,
                .input = macro->span.start,
                .len = macro->span.len,
            };

            size_t rid = insert_include(includes, &resc);

            push_elem_vec(incl_stack, &rid);

            // printf("Push %zu!\n", rid);
            // print_macros(*macro_table);

            lex = (Lex){0};
        }
    } else if (lex.type == LEX_MacroToken) {
        switch (lex.macro) {
        case Define:
            lex = process_define(input, len, idx, id_table, macro_table);
            break;
        case Include:
            lex = include_file(input, len, idx, incl_stack, includes, id_table);
            break;
        case Undefine: {
            // TODO: Find a neater way
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
                printf("#error %.*s before char %zu\n", (int)lex.span.len,
                       lex.span.start, lex.span.len);
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
                printf("#warning %.*s before char %zu\n", (int)lex.span.len,
                       lex.span.start, lex.span.len);
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

    if (lex.type == LEX_Eof && (*incl_stack)->length > 1) {
        pop_top_include(*incl_stack, *includes, *macro_table);
        lex = (Lex){0};
    }

    if (!lex.type && lex.invalid == Ok) {
        return preprocessed_lex_next(incl_stack, if_depth, id_table, includes,
                                     macro_table);
    }

    return lex;
}

Macros *create_macros(size_t capacity) {
    return create_dht(capacity, sizeof(size_t), sizeof(MacroDefine));
}

void print_macros(Macros *macro_table) {
    Entry entry;
    size_t idx = 0;
    while ((entry = next_elem_dht(macro_table, &idx)).key) {
        MacroDefine *macro = entry.value;
        printf("(id: %zu args: %p span: [%p, %zu])\n", *(size_t *)entry.key,
               macro->args, macro->span.start, macro->span.len);
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

Includes *create_includes(size_t capacity) {
    return create_vec(capacity, sizeof(IncludeResource));
}

// If exists reuse, otherwise push a new one
// NOTE: Due to being in use, function macros can allocate themselves a lot
// Consider a GC for them if there are too many allocs
size_t insert_include(Includes **includes, IncludeResource *resource) {
    if (resource->incl_type == IncludeFile) {
        for (size_t i = 0; i < (*includes)->length; i++) {
            IncludeResource *resc = at_elem_vec(*includes, i);
            if (resc->incl_type == IncludeFile && !resc->idx &&
                resc->path->length == resource->path->length &&
                !memcmp(resc->path->s, resource->path->s, resc->path->length)) {
                return i;
            }
        }
    } else if (resource->incl_type == IncludeMacro) {
        for (size_t i = 0; i < (*includes)->length; i++) {
            IncludeResource *resc = at_elem_vec(*includes, i);
            if (resc->incl_type == IncludeMacro && !resc->idx &&
                resc->macro_id == resource->macro_id &&
                resc->input == resource->input) {
                return i;
            }
        }
    }
    push_elem_vec(includes, resource);
    return (*includes)->length - 1;
}

void print_includes(Includes *includes) {
    for (size_t i = 0; i < includes->length; i++) {
        IncludeResource *resc = at_elem_vec(includes, i);
        if (resc->incl_type == IncludeFile) {
            printf("#path: %s ptr: %p len: %zu idx: %zu#\n", resc->path->s,
                   resc->input, resc->len, resc->idx);
        } else if (resc->incl_type == IncludeMacro) {
            printf("#mid: %zu ptr: %p len: %zu idx: %zu#\n", resc->macro_id,
                   resc->input, resc->len, resc->idx);
        }
    }
}

void free_includes(Includes *includes) {
    for (size_t i = 0; i < includes->length; i++) {
        IncludeResource *resc = at_elem_vec(includes, i);
        if (resc->incl_type == IncludeFile) {
            free(resc->input);
            delete_str(resc->path);
        }
    }
    free(includes);
}

IncludeStack *create_include_stack(size_t capacity) {
    return create_vec(capacity, sizeof(size_t));
}

IncludeResource *get_top_include(IncludeStack *incl_stack, Includes *includes) {
    size_t top_id = *(size_t *)peek_elem_vec(incl_stack);
    return at_elem_vec(includes, top_id);
}

void pop_top_include(IncludeStack *incl_stack, Includes *includes,
                     Macros *macros) {
    size_t top_id = *(size_t *)peek_elem_vec(incl_stack);
    pop_elem_vec(incl_stack);

    IncludeResource *resc = at_elem_vec(includes, top_id);
    resc->idx = 0;

    // printf("Pop %zu!\n", top_id);
    // print_macros(*macros);

    if (resc->incl_type == IncludeMacro) {
        MacroDefine *macro = get_elem_dht(macros, &resc->macro_id);
        if (macro && macro->args) {
            for (size_t i = 0; i < macro->args->length; i++) {
                // NOTE: We can safely delete,
                // these submacros cannot be function macros (ID(x,y))
                delete_elem_dht(macros, at_elem_vec(macro->args, i));
            }
        }
    }
}

void print_include_stack(IncludeStack *incl_stack) {
    for (size_t i = 0; i < incl_stack->length; i++) {
        size_t *id = at_elem_vec(incl_stack, i);
        printf("(include_id: %zu)\n", *id);
    }
}

void free_include_stack(IncludeStack *incl_stack) { free(incl_stack); }
