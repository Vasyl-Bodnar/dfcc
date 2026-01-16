#include "preprocessor.h"
#include "lexer.h"
#include "vec.h"

/*
enum include_type {
    Hheader,
    Qheader,
};

// Legal macro lexes
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

typedef struct Macro {
    enum macro_type type;
    union {
        struct {
            size_t id;
            Span span;
            Ids *args;
        } def;
        struct {
            Span span;
            MacroLexes *lexes;
        } cond;
        struct {
            enum include_type incl_type;
            size_t id;
        } incl;
        struct {
            size_t lineno;
            size_t id;
        } line;
        size_t defcond;
        size_t str;
        size_t pragma; // TODO: consider implementing pragmas
    };
} Macro;
*/

MacroDefine *get_macro(Macros **macro_table, size_t id) {
    for (size_t i = 0; i < (*macro_table)->length; i++) {
        MacroDefine *macro = ((MacroDefine *)(*macro_table)->v) + i;
        if (id == macro->id) {
            return macro;
        }
    }

    return 0;
}

uint32_t insert_or_update_macro(Macros **macro_table, MacroDefine macro) {
    for (size_t i = 0; i < (*macro_table)->length; i++) {
        MacroDefine *oth_macro = ((MacroDefine *)(*macro_table)->v) + i;
        if (macro.id == oth_macro->id) {
            memcpy(oth_macro, &macro, sizeof(MacroDefine));
            return 2;
        }
    }

    return push_elem_vec(macro_table, &macro);
}

Vector *process_define_args(const char *input, size_t len, size_t *idx,
                            Ids **id_table) {
    Vector *args = create_vec(0, sizeof(size_t));
    Lex cur = lex_next(input, len, idx, id_table);
    while (cur.type == Identifier) {
        push_elem_vec(&args, &cur.id);
        cur = lex_next(input, len, idx, id_table);
        if (cur.type == Comma) {
            cur = lex_next(input, len, idx, id_table);
        } else if (cur.type == RParen || cur.type == Eof) {
            return args;
        }
    }

    return 0;
}

// Handle variadic stuff
Lex process_define(const char *input, size_t len, size_t *idx, Ids **id_table,
                   Macros **macro_table) {
    MacroDefine macro = (MacroDefine){0};
    Lex result = (Lex){0};

    Lex id = lex_next(input, len, idx, id_table);
    if (id.type == Identifier) {
        macro.id = id.id;

        if (input[*idx] == '(') {
            *idx += 1;
            macro.args = process_define_args(input, len, idx, id_table);
            if (!macro.args) {
                return (Lex){.type = Invalid,
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
                    return (Lex){.type = Invalid,
                                 .span = id.span,
                                 .invalid = ExpectedNLAfterSlashMacroDefine};
                }
            } else if (input[span.start + span.len] == '\0') {
                result = (Lex){.type = Eof};
            }
        }
        macro.span = span;

        if (!insert_or_update_macro(macro_table, macro)) {
            return (Lex){
                .type = Invalid, .span = id.span, .invalid = FailedRealloc};
        }

        return result;
    }

    return (Lex){
        .type = Invalid, .span = id.span, .invalid = ExpectedIdMacroDefine};
}

// Consider doing something wiser than just lexing everything needlessly
// Really we just need to jump to the next define
Lex conditional_exclude(const char *input, size_t len, size_t *idx,
                        size_t *if_depth, Ids **id_table,
                        Macros **macro_table) {
    Lex lex = (Lex){0};
    while (lex.type != Eof) {
        while ((lex = lex_next(input, len, idx, id_table)).type == MacroToken)
            ;
        switch (lex.macro) {
        case If:
        case IfDefined:
        case IfNotDefined:
            *if_depth += 1;
        case Else:
        case ElseIf:
        case ElseIfDefined:
        case ElseIfNotDefined:
            // TODO: Handle
        case EndIf:
            if (!(*if_depth))
                goto Exit_if_loop;
            *if_depth -= 1;
        default:
            continue;
        }
    }
    lex = (Lex){0};
Exit_if_loop:
    return lex;
}

// TODO: Macros should be first characters of the line.
//       Lexer does not handle this currently
Lex preprocessed_lex_next(const char *input, size_t len, size_t *idx,
                          size_t *if_depth, Ids **id_table,
                          Macros **macro_table) {
    Lex lex = lex_next(input, len, idx, id_table);

    if (lex.type == Identifier) {
        MacroDefine *macro = get_macro(macro_table, lex.id);
        if (macro) {
            // TODO: handle macro define expansion
        }
    } else if (lex.type == MacroToken) {
        switch (lex.macro) {
        case Define:
            lex = process_define(input, len, idx, id_table, macro_table);
        case Include:
        case Undefine:
        case If:
            lex = (Lex){0};
            break;
        case IfDefined:
            lex = lex_next(input, len, idx, id_table);
            if (lex.type == Identifier) {
                MacroDefine *macro = get_macro(macro_table, lex.id);
                if (macro) {
                    *if_depth += 1;
                    lex = (Lex){0};
                } else {
                    lex = conditional_exclude(input, len, idx, if_depth,
                                              id_table, macro_table);
                }
            } else {
                return (Lex){.type = Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIdIfDef};
            }
            break;
        case IfNotDefined:
        case Else:
        case ElseIf:
        case ElseIfDefined:
        case ElseIfNotDefined:
            lex = (Lex){0};
            break;
        case EndIf:
            if (!(*if_depth)) {
                return (Lex){.type = Invalid,
                             .span = lex.span,
                             .invalid = ExpectedIfEndIf};
            }
            *if_depth -= 1;
            lex = (Lex){0};
            break;
        case Line:
        case Embed:
        case Error:
        case Warning:
        case Pragma:
            lex = (Lex){0};
            break;
        }
    }

    if (!lex.type && lex.invalid == Ok) {
        return preprocessed_lex_next(input, len, idx, if_depth, id_table,
                                     macro_table);
    }

    return lex;
}
