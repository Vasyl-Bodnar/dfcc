#include "pp.h"
#include "got.h"
#include "lexer.h"
#include "vec.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

Lex lex_next_top_expand(Preprocessor *pp, Ids **id_table);
IncludeResource *get_top_resc(Preprocessor *pp);

Lex macro_include_file(Preprocessor *pp, Ids **id_table) {
    IncludeResource *top = get_top_resc(pp);
    if (top->type == IncludeMacro) {
        return (Lex){.type = LEX_Invalid, .invalid = ExpectedFileNotMacro};
    }

    Lex lex = lex_next(&top->stream, id_table);
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

        realpath(top->path->s, path);
        size_t path_len = strlen(path);

        for (; path[path_len] != '/'; path_len--)
            ;

        memcpy(path + path_len + 1, str.start, str.len);
        path[path_len + 1 + str.len] = '\0';

        String *file_path = from_cstr(path);

        return include_file(pp, file_path, id_table);
    } else if (lex.type == LEX_String) {
        char path[PATH_MAX];
        Span str = *(Span *)at_elem_vec(*id_table, lex.id);

        realpath(top->path->s, path);
        size_t path_len = strlen(path);

        for (; path[path_len] != '/'; path_len--)
            ;

        memcpy(path + path_len + 1, str.start + 1, str.len - 2);
        path[path_len + str.len - 1] = '\0';

        String *file_path = from_cstr(path);

        return include_file(pp, file_path, id_table);
    } else {
        return (Lex){.type = LEX_Invalid,
                     .span = lex.span,
                     .invalid = ExpectedIncludeHeader};
    }
}

// We get LEX_OK when including a file, which is not really bad, but interesting
Lex lex_next_top(Preprocessor *pp, Ids **id_table) {
    if (pp->incl_stack->length) {
        size_t *idx = at_elem_vec(pp->incl_stack, pp->incl_stack->length - 1);
        IncludeResource *resc = at_elem_vec(pp->incl_table, *idx);

        Lex lex;
        if (resc->type == IncludeFile) {
            lex = lex_next(&resc->stream, id_table);
        } else {
            lex = *(Lex *)at_elem_vec(resc->lexes, resc->idx++);
        }

        if (lex.type == LEX_Eof) {
            pop_elem_vec(pp->incl_stack);
            return lex_next_top(pp, id_table);
        }

        return lex;
    }
    return (Lex){.type = LEX_Eof};
}

Lex lex_next_top_expand(Preprocessor *pp, Ids **id_table) {
    Lex lex = lex_next_top(pp, id_table);
    if (lex.type == LEX_Identifier) {
        DefineMacro *macro = get_elem_dht(pp->macro_table, &lex.id);
        if (macro) {
            return include_macro(pp, lex.id, macro, id_table);
        }
    }
    return lex;
}

Lex pp_lex_next(Preprocessor *pp, Ids **id_table) {
    Lex lex = lex_next_top_expand(pp, id_table);
    if (lex.type == LEX_MacroToken) {
        switch (lex.macro) {
        case Include:
            return macro_include_file(pp, id_table);
        case Define:
        case Undefine:
        case If:
        case IfDefined:
        case IfNotDefined:
        case Else:
        case ElseIf:
        case ElseIfDefined:
        case ElseIfNotDefined:
        case EndIf:
        case Line:
        case Embed:
        case Error:
        case Warning:
        case Pragma:
            return lex;
        }
    }
    return lex;
}

// TODO: Is this the right way?
int top_macro_line(Preprocessor *pp) {
    if (pp->incl_table->length) {
        size_t idx =
            *(size_t *)at_elem_vec(pp->incl_table, pp->incl_table->length - 1);
        IncludeResource *resc = at_elem_vec(pp->incl_table, idx);
        if (resc->type == IncludeFile) {
            return resc->stream.macro_line;
        } else {
            return resc->macro_line;
        }
    }
    return 0;
}

// TODO: Incomplete
Lex define_macro(Preprocessor *pp, Ids **id_table) {
    Lexes *lexes = create_lexes(1);
    Lex lex;
    do {
        lex = lex_next_top_expand(pp, id_table);
        push_elem_vec(&lexes, &lex);
    } while (top_macro_line(pp));
    return lex;
}

void insert_include_macro(Preprocessor *pp, IncludeResource *macro_resc) {
    for (size_t i = 0; i < pp->incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(pp->incl_table, i);
        if (resc->type == IncludeMacro && !resc->idx &&
            resc->mid == macro_resc->mid && resc->lexes == macro_resc->lexes) {
            push_elem_vec(&pp->incl_stack, &i);
            return;
        }
    }
    size_t id = pp->incl_table->length;
    push_elem_vec(&pp->incl_table, macro_resc);
    push_elem_vec(&pp->incl_stack, &id);
}

// TODO: Optimize redundant includes
Lex include_macro(Preprocessor *pp, size_t mid, DefineMacro *macro,
                  Ids **id_table) {
    if (macro->args) {
        return (Lex){.type = LEX_Invalid, .invalid = ExpectedValidIncludeMacro};
    }

    insert_include_macro(pp, &(IncludeResource){
                                 .type = IncludeMacro,
                                 .mid = mid,
                                 .macro_line = top_macro_line(pp),
                                 .idx = 0,
                                 .lexes = macro->lexes,
                             });

    return lex_next_top_expand(pp, id_table);
}

Lex include_file(Preprocessor *pp, String *path, Ids **id_table) {
    // If already present and unused, we don't need to allocate again
    for (size_t i = 0; i < pp->incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(pp->incl_table, i);
        if (resc->type == IncludeFile && !resc->stream.idx &&
            resc->path->length == path->length &&
            !memcmp(resc->path->s, path->s, resc->path->length)) {
            push_elem_vec(&pp->incl_stack, &i);
            return lex_next_top_expand(pp, id_table);
        }
    }

    FILE *file = fopen(path->s, "r");
    if (!file) {
        puts("Such a file could not be found");
        return (Lex){.type = LEX_Invalid, .invalid = ExpectedValidIncludeFile};
    }

    struct stat st_buf;
    fstat(fileno(file), &st_buf);

    char *buf = malloc(st_buf.st_size + 1);
    size_t bytes = fread(buf, 1, st_buf.st_size, file);
    fclose(file);

    if (bytes == 0) {
        free(buf);
        return (Lex){.type = LEX_Invalid, .invalid = ExpectedValidIncludeFile};
    }

    if (buf[bytes - 1] != '\\') {
        buf[bytes] = '\n';
    } else {
        puts("Last character cannot be '\\'");
        free(buf);
        return (Lex){.type = LEX_Invalid, .invalid = ExpectedValidIncludeFile};
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

    return (Lex){0};
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
        printf("#string_id: %zu args: %p", *(size_t *)entry.key, macro->args);
        if (macro->lexes) {
            printf(" elems-len: %zu elems:\n", macro->lexes->length);
            print_lexes(macro->lexes);
        }
        printf("#\n");
    }
}

void print_incl_table(Includes *incl_table) {
    for (size_t i = 0; i < incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(incl_table, i);
        if (resc->type == IncludeFile) {
            printf(
                "<path: %s base-ptr: %p len: %zu row: %zu col: %zu idx: %zu>\n",
                resc->path->s, resc->stream.start, resc->stream.len,
                resc->stream.row, resc->stream.col, resc->stream.idx);
        } else if (resc->type == IncludeMacro) {
            printf("<mid: %zu token-idx: %zu", resc->mid, resc->idx);
            if (resc->lexes) {
                printf(" elems-len: %zu elems:\n", resc->lexes->length);
                print_lexes(resc->lexes);
            }
            printf(">\n");
        }
    }
}

void print_incl_stack(IncludeStack *incl_stack) {
    for (size_t i = 0; i < incl_stack->length; i++) {
        size_t *id = at_elem_vec(incl_stack, i);
        printf("(include-idx: %zu)\n", *id);
    }
}

Preprocessor *create_pp() {
    Preprocessor *pp = malloc(sizeof(*pp));
    pp->incl_table = create_vec(4, sizeof(IncludeResource));
    pp->incl_stack = create_vec(4, sizeof(size_t));
    pp->macro_table = create_dht(8, sizeof(size_t), sizeof(DefineMacro));
    pp->macro_if_depth = 0;
    return pp;
}

void print_pp(Preprocessor *pp) {
    printf("(Preprocessor: macro-if-depth: %zu\n", pp->macro_if_depth);
    printf("include-stack:\n");
    print_incl_stack(pp->incl_stack);
    printf("include-table:\n");
    print_incl_table(pp->incl_table);
    printf("defined-macros:\n");
    print_macro_table(pp->macro_table);
    printf(")\n");
}

void delete_pp(Preprocessor *pp) {
    for (size_t i = 0; i < pp->incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(pp->incl_table, i);
        if (resc->type == IncludeFile) {
            delete_str(resc->path);
            free(resc->stream.start);
        } else {
            delete_vec(resc->lexes);
        }
    }
    delete_vec(pp->incl_table);
    delete_vec(pp->incl_stack);
    delete_dht(pp->macro_table);
    free(pp);
}
