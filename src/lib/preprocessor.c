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

Lex if_defined(Stream *stream, size_t *if_depth, Ids **id_table,
               Macros **macro_table);
Lex if_not_defined(Stream *stream, size_t *if_depth, Ids **id_table,
                   Macros **macro_table);
Lex include_macro(MacroDefine *macro, size_t mid, Includes **includes,
                  IncludeStack **incl_stack, Macros **macro_table,
                  Ids **id_table);

Vector *process_define_args(Stream *stream, Ids **id_table) {
    Vector *args = create_vec(0, sizeof(size_t));
    Lex cur = lex_next(stream, id_table);
    while (cur.type == LEX_Identifier) {
        push_elem_vec(&args, &cur.id);
        cur = lex_next(stream, id_table);
        if (cur.type == LEX_Comma) {
            cur = lex_next(stream, id_table);
        } else if (cur.type == LEX_RParen || cur.type == LEX_Eof) {
            return args;
        }
    }

    return 0;
}

// TODO: Handle variadic ...
Lex process_define(IncludeResource *resc, Ids **id_table, Includes **includes,
                   IncludeStack **incl_stack, Macros **macro_table) {
    if (resc->incl_type == IncludeMacro) {
        printf("3 ");
        return (Lex){
            .type = LEX_Invalid, .span = {0}, .invalid = ExpectedFileNotMacro};
    }

    Stream *stream = &resc->input;
    MacroDefine macro = (MacroDefine){.args = 0, .lexes = 0};
    Lex result = (Lex){0};

    Lex id = lex_next(stream, id_table);
    if (id.type == LEX_Identifier) {
        size_t mid = id.id;

        if (stream->start[stream->idx] == '(') {
            stream->idx += 1;
            stream->col += 1;
            macro.args = process_define_args(stream, id_table);
            if (!macro.args) {
                return (Lex){.type = LEX_Invalid,
                             .span = id.span,
                             .invalid = ExpectedGoodArgsMacroDefine};
            }
        }

        Stream saved = *stream;
        Span span = from_stream(stream, 1);
        while (span.start[span.len] != '\n') {
            stream->col += 1;
            span.len += 1;
            if (span.start[span.len] == '\\') {
                if (span.start[span.len + 1] == '\n') {
                    stream->row += 1;
                    stream->col = 0;
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
        stream->idx += span.len;
        saved.len = stream->idx;

        macro.lexes = create_lexes(1);
        Lex tok = {0};
        while (tok.type != LEX_Eof) {
            tok = lex_next(&saved, id_table);
            if (tok.type == LEX_Identifier) {
                MacroDefine *macro = get_elem_dht(*macro_table, &tok.id);
                if (macro) {
                    tok = include_macro(macro, tok.id, includes, incl_stack,
                                        macro_table, id_table);
                }
            }
            if (tok.type || tok.invalid) {
                push_elem_vec(&macro.lexes, &tok);
            }
        }

        if (!put_elem_dht(macro_table, &mid, &macro)) {
            return (Lex){
                .type = LEX_Invalid, .span = id.span, .invalid = FailedRealloc};
        }

        return result;
    }

    return (Lex){
        .type = LEX_Invalid, .span = id.span, .invalid = ExpectedIdMacroDefine};
}

Lex conditional_exclude_tail(Stream *stream, size_t *if_depth, Ids **id_table) {
    size_t cur_if_depth = *if_depth;
    Lex lex = (Lex){0};
    while (lex.type != LEX_Eof) {
        while ((lex = lex_next(stream, id_table)).type != LEX_MacroToken)
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
Lex conditional_exclude(Stream *stream, size_t *if_depth, Ids **id_table,
                        Macros **macro_table) {
    size_t cur_if_depth = *if_depth;
    Lex lex = (Lex){0};
    while (lex.type != LEX_Eof) {
        while ((lex = lex_next(stream, id_table)).type != LEX_MacroToken)
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
                return if_defined(stream, if_depth, id_table, macro_table);
            }
            break;
        case ElseIfNotDefined:
            if (*if_depth == cur_if_depth) {
                return if_not_defined(stream, if_depth, id_table, macro_table);
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

Lex if_defined(Stream *stream, size_t *if_depth, Ids **id_table,
               Macros **macro_table) {
    Lex lex = lex_next(stream, id_table);
    if (lex.type == LEX_Identifier) {
        MacroDefine *macro = get_elem_dht(*macro_table, &lex.id);
        *if_depth += 1;
        if (macro) {
            lex = (Lex){0};
        } else {
            lex = conditional_exclude(stream, if_depth, id_table, macro_table);
        }
    } else {
        return (Lex){
            .type = LEX_Invalid, .span = lex.span, .invalid = ExpectedIdIfDef};
    }

    return lex;
}

Lex if_not_defined(Stream *stream, size_t *if_depth, Ids **id_table,
                   Macros **macro_table) {
    Lex lex = lex_next(stream, id_table);
    if (lex.type == LEX_Identifier) {
        MacroDefine *macro = get_elem_dht(*macro_table, &lex.id);
        *if_depth += 1;
        if (macro) {
            lex = conditional_exclude(stream, if_depth, id_table, macro_table);
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
        if (resc->incl_type == IncludeFile && !resc->input.idx &&
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

    push_elem_vec(
        includes,
        &(IncludeResource){
            .incl_type = IncludeFile,
            .path = file_path,
            .input = {.start = buf, .len = bytes, .idx = 0, .row = 1, .col = 0},
        });

    size_t new_top_id = (*includes)->length - 1;
    push_elem_vec(incl_stack, &new_top_id);
    return 1;
}

Lex include_fil(Stream *stream, IncludeStack **incl_stack, Includes **includes,
                Ids **id_table) {
    Lex lex = lex_next(stream, id_table);
    if (lex.type == LEX_Left) {
        Span str = {.start = lex.span.start + 1, .len = 0};
        while (str.start[str.len] != '>') {
            if (str.start[str.len] == '\n') {
                return (Lex){.type = LEX_Invalid,
                             .span = str,
                             .invalid = ExpectedValidIncludeFile};
            }
            str.len += 1;
            stream->idx += 1;
            stream->col += 1;
        }
        stream->idx += 1;
        stream->col += 1;

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

Lex lex_next_resc(IncludeResource *resc, Ids **id_table) {
    if (resc->incl_type == IncludeFile) {
        return lex_next(&resc->input, id_table);
    } else if (resc->incl_type == IncludeMacro) {
        if (resc->idx >= resc->lexes->length) {
            return (Lex){.type = LEX_Eof};
        }
        Lex lex = *(Lex *)at_elem_vec(resc->lexes, resc->idx);
        resc->idx += 1;
        return lex;
    }
    return (Lex){
        .type = LEX_Invalid, .span = {0}, .invalid = ExpectedImpossible};
}

Lex lex_next_top_resc(IncludeStack *incl_stack, Includes *includes,
                      Macros *macro_table, Ids **id_table) {
    IncludeResource *resc = get_top_include(incl_stack, includes);
    Lex lex = lex_next_resc(resc, id_table);
    if (lex.type == LEX_Eof && incl_stack->length) {
        pop_top_include(incl_stack, includes, macro_table);
        return lex_next_top_resc(incl_stack, includes, macro_table, id_table);
    } else {
        return lex;
    }
}

Lex include_macro(MacroDefine *macro, size_t mid, Includes **includes,
                  IncludeStack **incl_stack, Macros **macro_table,
                  Ids **id_table) {
    if (macro->args) {
        Lex lex =
            lex_next_top_resc(*incl_stack, *includes, *macro_table, id_table);
        if (lex.type == LEX_LParen) {
            for (size_t i = 0; i < macro->args->length; i++) {
                size_t aid = *(size_t *)at_elem_vec(macro->args, i);
                MacroDefine mac = {.args = 0, .lexes = create_lexes(2)};
                lex = lex_next_top_resc(*incl_stack, *includes, *macro_table,
                                        id_table);

                while (1) {
                    lex = lex_next_top_resc(*incl_stack, *includes,
                                            *macro_table, id_table);
                    if (lex.type == LEX_Identifier) {
                        MacroDefine *macro =
                            get_elem_dht(*macro_table, &lex.id);
                        if (macro) {
                            lex = include_macro(macro, lex.id, includes,
                                                incl_stack, macro_table,
                                                id_table);
                        }
                    }
                    if (lex.type == LEX_Eof) {
                        return (Lex){.type = LEX_Invalid,
                                     .span = lex.span,
                                     .invalid = ExpectedMoreArgsMacro};
                    }
                    if (lex.type == LEX_Comma || lex.type == LEX_RParen) {
                        break;
                    }
                    if (lex.type || lex.invalid) {
                        push_elem_vec(&mac.lexes, &lex);
                    }
                }

                put_elem_dht(macro_table, &aid, &mac);

                if (lex.type == LEX_RParen && i + 1 < macro->args->length) {
                    return (Lex){.type = LEX_Invalid,
                                 .span = lex.span,
                                 .invalid = ExpectedLessArgsMacro};
                }
            }

            if (lex.type != LEX_RParen) {
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

    IncludeResource mresc = {
        .incl_type = IncludeMacro,
        .macro_id = mid,
        .idx = 0,
        .lexes = macro->lexes,
    };

    size_t rid = insert_include(includes, &mresc);

    push_elem_vec(incl_stack, &rid);

    // printf("Push %zu!\n", rid);
    // print_macros(*macro_table);

    return (Lex){0};
}

// TODO: Macros should be first characters of the line.
//       Lexer does not handle this currently
Lex preprocessed_lex_next(IncludeStack **incl_stack, size_t *if_depth,
                          Ids **id_table, Includes **includes,
                          Macros **macro_table) {
    IncludeResource *resc = get_top_include(*incl_stack, *includes);
    Lex lex = lex_next_resc(resc, id_table);

    if (lex.type == LEX_Identifier) {
        MacroDefine *macro = get_elem_dht(*macro_table, &lex.id);
        if (macro) {
            lex = include_macro(macro, lex.id, includes, incl_stack,
                                macro_table, id_table);
        }
    } else if (lex.type == LEX_MacroToken) {
        if (resc->incl_type == IncludeMacro) {
            printf("2 ");
            lex = (Lex){.type = LEX_Invalid,
                        .span = {0},
                        .invalid = ExpectedFileNotMacro};
        } else {
            Stream *stream = &resc->input;
            switch (lex.macro) {
            case Define:
                lex = process_define(resc, id_table, includes, incl_stack,
                                     macro_table);
                break;
            case Include:
                lex = include_fil(stream, incl_stack, includes, id_table);
                break;
            case Undefine: {
                // TODO: Find a neater way
                MacroDefine *macro = get_elem_dht(*macro_table, &lex.id);
                if (macro) {
                    free(macro->args);  // Vector
                    free(macro->lexes); // Vector
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
                lex = if_defined(stream, if_depth, id_table, macro_table);
                break;
            case IfNotDefined:
                lex = if_not_defined(stream, if_depth, id_table, macro_table);
                break;
            case Else:
                if (!(*if_depth)) {
                    return (Lex){.type = LEX_Invalid,
                                 .span = lex.span,
                                 .invalid = ExpectedIfElse};
                }
                lex = conditional_exclude_tail(stream, if_depth, id_table);
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
                lex = if_defined(stream, if_depth, id_table, macro_table);
                break;
            case ElseIfNotDefined:
                if (!(*if_depth)) {
                    return (Lex){.type = LEX_Invalid,
                                 .span = lex.span,
                                 .invalid = ExpectedIfElseNotDef};
                }
                lex = if_not_defined(stream, if_depth, id_table, macro_table);
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
                lex = lex_next(stream, id_table);
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
                lex = lex_next(stream, id_table);
                if (lex.type == LEX_String) {
                    printf("#warning %.*s on line %zu\n", (int)lex.span.len,
                           lex.span.start, lex.span.row);
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
        printf("(id: %zu args: %p", *(size_t *)entry.key, macro->args);
        if (macro->lexes) {
            printf(" len: %zu elems:\n", macro->lexes->length);
            print_lexes(macro->lexes);
        }
        printf(")\n");
    }
}

void free_macros(Macros *macro_table) {
    Entry entry;
    size_t idx = 0;
    while ((entry = next_elem_dht(macro_table, &idx)).key) {
        free(((MacroDefine *)entry.value)->args);
        free(((MacroDefine *)entry.value)->lexes);
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
            if (resc->incl_type == IncludeFile && !resc->input.idx &&
                resc->path->length == resource->path->length &&
                !memcmp(resc->path->s, resource->path->s, resc->path->length)) {
                return i;
            }
        }
    } else if (resource->incl_type == IncludeMacro) {
        for (size_t i = 0; i < (*includes)->length; i++) {
            IncludeResource *resc = at_elem_vec(*includes, i);
            if (resc->incl_type == IncludeMacro && !resc->input.idx &&
                resc->macro_id == resource->macro_id &&
                resc->input.start == resource->input.start) {
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
            printf("#path: %s ptr: %p len: %zu row: %zu col: %zu idx: %zu#\n",
                   resc->path->s, resc->input.start, resc->input.len,
                   resc->input.row, resc->input.col, resc->input.idx);
        } else if (resc->incl_type == IncludeMacro) {
            printf("#mid: %zu idx: %zu", resc->macro_id, resc->idx);
            if (resc->lexes) {
                printf(" len: %zu elems:\n", resc->lexes->length);
                print_lexes(resc->lexes);
            }
            printf("#\n");
        }
    }
}

void free_includes(Includes *includes) {
    for (size_t i = 0; i < includes->length; i++) {
        IncludeResource *resc = at_elem_vec(includes, i);
        if (resc->incl_type == IncludeFile) {
            free((char *)resc->input.start);
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

    // printf("Pop %zu!\n", top_id);
    // print_macros(*macros);
    if (resc->incl_type == IncludeFile) {
        resc->input.idx = 0;
    } else if (resc->incl_type == IncludeMacro) {
        resc->idx = 0;
    }
}

void print_include_stack(IncludeStack *incl_stack) {
    for (size_t i = 0; i < incl_stack->length; i++) {
        size_t *id = at_elem_vec(incl_stack, i);
        printf("(include_id: %zu)\n", *id);
    }
}

void free_include_stack(IncludeStack *incl_stack) { free(incl_stack); }
