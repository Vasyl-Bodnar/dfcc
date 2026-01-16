/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lexer.h"
#include <math.h>
#include <stdlib.h>

uint8_t key_len_table[KEY_COUNT] = {
    [KEY_alignas] = 7,
    [KEY_alignof] = 7,
    [KEY_auto] = 4,
    [KEY_bool] = 4,
    [KEY_break] = 5,
    [KEY_case] = 4,
    [KEY_char] = 4,
    [KEY_const] = 5,
    [KEY_constexpr] = 9,
    [KEY_continue] = 8,
    [KEY_default] = 7,
    [KEY_do] = 2,
    [KEY_double] = 6,
    [KEY_else] = 4,
    [KEY_enum] = 4,
    [KEY_extern] = 6,
    [KEY_false] = 5,
    [KEY_float] = 5,
    [KEY_for] = 3,
    [KEY_goto] = 4,
    [KEY_if] = 2,
    [KEY_inline] = 6,
    [KEY_int] = 3,
    [KEY_long] = 4,
    [KEY_nullptr] = 7,
    [KEY_register] = 8,
    [KEY_restrict] = 8,
    [KEY_return] = 6,
    [KEY_short] = 5,
    [KEY_signed] = 6,
    [KEY_sizeof] = 6,
    [KEY_static] = 6,
    [KEY_static_assert] = 13,
    [KEY_struct] = 6,
    [KEY_switch] = 6,
    [KEY_thread_local] = 12,
    [KEY_true] = 4,
    [KEY_typedef] = 7,
    [KEY_typeof] = 6,
    [KEY_typeof_unqual] = 13,
    [KEY_union] = 5,
    [KEY_unsigned] = 8,
    [KEY_void] = 4,
    [KEY_volatile] = 8,
    [KEY_while] = 5,
    [KEY__Atomic] = 7,
    [KEY__BitInt] = 7,
    [KEY__Complex] = 8,
    [KEY__Decimal128] = 11,
    [KEY__Decimal32] = 10,
    [KEY__Decimal64] = 10,
    [KEY__Generic] = 8,
    [KEY__Imaginary] = 10,
    [KEY__Noreturn] = 9,
};

int nondigit(char c) {
    return ((c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

int digit(char c) { return c >= '0' && c <= '9'; }
int oct_digit(char c) { return c >= '0' && c <= '7'; }
int hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}
int bin_digit(char c) { return c == '0' || c == '1'; }
int nonzero(char c) { return c >= '1' && c <= '9'; }
uint64_t acquire(char c) { return c - '0'; }
uint64_t hex_acquire(char c) {
    if (digit(c)) {
        return c - '0';
    } else if (c >= 'a') {
        return c - 'a';
    } else {
        return c - 'A';
    }
}

int nondigit_or_digit(char c) {
    return ((c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9'));
}

int punctuation(char c) {
    return c == '[' || c == ']' || c == '(' || c == ')' || c == '{' ||
           c == '}' || c == '.' || c == '&' || c == '*' || c == '+' ||
           c == '-' || c == '~' || c == '!' || c == '/' || c == '%' ||
           c == '<' || c == '>' || c == '^' || c == '|' || c == '?' ||
           c == ':' || c == ';' || c == '=' || c == ',' || c == '#';
}

int space(char c) { return c == ' ' || c == '\n' || c == '\t' || c == '\r'; }

// Check if this id already exists, else push it on
size_t search_id_table(const char *input, Span span, Ids **id_table) {
    for (size_t i = 0; i < (*id_table)->length; i++) {
        Span id = ((Span *)(*id_table)->v)[i];
        if (span.len == id.len &&
            !memcmp(input + id.start, input + span.start, span.len)) {
            return i;
        }
    }
    push_elem_vec(id_table, &span);
    return (*id_table)->length - 1;
}

Lex check_keyword(const char *input, Span span) {
    if (span.len < 2 || span.len > 13) {
        return (Lex){0};
    }
    switch (input[span.start]) {
    case 'a':
        if (span.len == key_len_table[KEY_auto] &&
            !memcmp("auto", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_auto};
        } else if (span.len == key_len_table[KEY_alignas] &&
                   !memcmp("alignas", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_alignas};
        } else if (span.len == key_len_table[KEY_alignof] &&
                   !memcmp("alignof", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_alignof};
        }
        break;
    case 'b':
        if (span.len == key_len_table[KEY_break] &&
            !memcmp("break", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_break};
        } else if (span.len == key_len_table[KEY_bool] &&
                   !memcmp("bool", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_bool};
        }
        break;
    case 'c':
        if (span.len < 4 || span.len > 9) {
            break;
        } else if (span.len == key_len_table[KEY_continue] &&
                   !memcmp("continue", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_continue};
        } else if (span.len == key_len_table[KEY_constexpr] &&
                   !memcmp("constexpr", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_constexpr};
        } else if (span.len == key_len_table[KEY_const] &&
                   !memcmp("const", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_const};
        } else if (span.len == key_len_table[KEY_char] &&
                   !memcmp("char", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_char};
        } else if (span.len == key_len_table[KEY_case] &&
                   !memcmp("case", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_case};
        }
        break;
    case 'd':
        if (span.len == key_len_table[KEY_do] &&
            !memcmp("do", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_do};
        } else if (span.len == key_len_table[KEY_double] &&
                   !memcmp("double", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_double};
        } else if (span.len == key_len_table[KEY_default] &&
                   !memcmp("default", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_default};
        }
        break;
    case 'e':
        if (span.len == key_len_table[KEY_extern] &&
            !memcmp("extern", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_extern};
        } else if (span.len == key_len_table[KEY_enum] &&
                   !memcmp("enum", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_enum};
        } else if (span.len == key_len_table[KEY_else] &&
                   !memcmp("else", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_else};
        }
        break;
    case 'f':
        if (span.len == key_len_table[KEY_for] &&
            !memcmp("for", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_for};
        } else if (span.len == key_len_table[KEY_float] &&
                   !memcmp("float", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_float};
        } else if (span.len == key_len_table[KEY_false] &&
                   !memcmp("false", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_false};
        }
        break;
    case 'g':
        if (span.len == key_len_table[KEY_goto] &&
            !memcmp("goto", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_goto};
        }
        break;
    case 'i':
        if (span.len == key_len_table[KEY_int] &&
            !memcmp("int", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_int};
        } else if (span.len == key_len_table[KEY_inline] &&
                   !memcmp("inline", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_inline};
        } else if (span.len == key_len_table[KEY_if] &&
                   !memcmp("if", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_if};
        }
        break;
    case 'l':
        if (span.len == key_len_table[KEY_long] &&
            !memcmp("long", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_long};
        }
        break;
    case 'n':
        if (span.len == key_len_table[KEY_nullptr] &&
            !memcmp("nullptr", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_nullptr};
        }
        break;
    case 'r':
        if (span.len == key_len_table[KEY_return] &&
            !memcmp("return", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_return};
        } else if (span.len == key_len_table[KEY_restrict] &&
                   !memcmp("restrict", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_restrict};
        } else if (span.len == key_len_table[KEY_register] &&
                   !memcmp("register", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_register};
        }
        break;
    case 's':
        if (span.len < 5) {
            break;
        } else if (span.len == key_len_table[KEY_switch] &&
                   !memcmp("switch", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_switch};
        } else if (span.len == key_len_table[KEY_struct] &&
                   !memcmp("struct", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_struct};
        } else if (span.len == key_len_table[KEY_static_assert] &&
                   !memcmp("static_assert", input + span.start, span.len)) {
            return (Lex){
                .type = Keyword, .span = span, .key = KEY_static_assert};
        } else if (span.len == key_len_table[KEY_static] &&
                   !memcmp("static", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_static};
        } else if (span.len == key_len_table[KEY_sizeof] &&
                   !memcmp("sizeof", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_sizeof};
        } else if (span.len == key_len_table[KEY_signed] &&
                   !memcmp("signed", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_signed};
        } else if (span.len == key_len_table[KEY_short] &&
                   !memcmp("short", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_short};
        }
        break;
    case 't':
        if (span.len < 4) {
            break;
        } else if (span.len == key_len_table[KEY_typeof_unqual] &&
                   !memcmp("typeof_unqual", input + span.start, span.len)) {
            return (Lex){
                .type = Keyword, .span = span, .key = KEY_typeof_unqual};
        } else if (span.len == key_len_table[KEY_typeof] &&
                   !memcmp("typeof", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_typeof};
        } else if (span.len == key_len_table[KEY_typedef] &&
                   !memcmp("typedef", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_typedef};
        } else if (span.len == key_len_table[KEY_true] &&
                   !memcmp("true", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_true};
        } else if (span.len == key_len_table[KEY_thread_local] &&
                   !memcmp("thread_local", input + span.start, span.len)) {
            return (Lex){
                .type = Keyword, .span = span, .key = KEY_thread_local};
        }
        break;
    case 'u':
        if (span.len == key_len_table[KEY_union] &&
            !memcmp("union", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_union};
        } else if (span.len == key_len_table[KEY_unsigned] &&
                   !memcmp("unsigned", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_unsigned};
        }
        break;
    case 'v':
        if (span.len == key_len_table[KEY_void] &&
            !memcmp("void", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_void};
        } else if (span.len == key_len_table[KEY_volatile] &&
                   !memcmp("volatile", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_volatile};
        }
        break;
    case 'w':
        if (span.len == key_len_table[KEY_while] &&
            !memcmp("while", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY_while};
        }
        break;
    case '_':
        if (span.len < 7 || span.len > 11) {
            break;
        } else if (span.len == key_len_table[KEY__Noreturn] &&
                   !memcmp("_Noreturn", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY__Noreturn};
        } else if (span.len == key_len_table[KEY__Imaginary] &&
                   !memcmp("_Imaginary", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY__Imaginary};
        } else if (span.len == key_len_table[KEY__Generic] &&
                   !memcmp("_Generic", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY__Generic};
        } else if (span.len == key_len_table[KEY__Decimal32] &&
                   !memcmp("_Decimal32", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY__Decimal32};
        } else if (span.len == key_len_table[KEY__Decimal64] &&
                   !memcmp("_Decimal64", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY__Decimal64};
        } else if (span.len == key_len_table[KEY__Decimal128] &&
                   !memcmp("_Decimal128", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY__Decimal128};
        } else if (span.len == key_len_table[KEY__Complex] &&
                   !memcmp("_Complex", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY__Complex};
        } else if (span.len == key_len_table[KEY__BitInt] &&
                   !memcmp("_BitInt", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY__BitInt};
        } else if (span.len == key_len_table[KEY__Atomic] &&
                   !memcmp("_Atomic", input + span.start, span.len)) {
            return (Lex){.type = Keyword, .span = span, .key = KEY__Atomic};
        }
        break;
    default:
        break;
    }
    return (Lex){0};
}

uint32_t check_string_prefix(const char *input, Span span) {
    return ((span.len == 2 && input[span.start] == 'u' &&
             input[span.start + 1] == '8' &&
             (input[span.start + 2] == '\'' || input[span.start + 2] == '"')) ||
            (span.len == 1 &&
             (input[span.start] == 'u' || input[span.start] == 'U' ||
              input[span.start] == 'L') &&
             (input[span.start + 1] == '\'' || input[span.start + 1] == '"')));
}

// MINOR: Possibly handle XID_Start and XID_Continue
Lex keyword_or_id(const char *input, size_t idx, Vector **id_table) {
    if (nondigit(input[idx])) {
        Span span = {idx, 1};

        while (nondigit_or_digit(input[span.start + span.len])) {
            span.len += 1;
        }

        Lex keyword = check_keyword(input, span);
        if (keyword.type) {
            return keyword;
        }

        if (check_string_prefix(input, span)) {
            return (Lex){0};
        }

        return (Lex){.type = Identifier,
                     .span = span,
                     .id = search_id_table(input, span, id_table)};
    }
    return (Lex){0};
}

Lex macro(const char *input, size_t idx, size_t limit, Vector **id_table) {
    if (limit > 1) {
        if (nondigit(input[idx + 1])) {
            Span span = {idx, 2};

            while (nondigit(input[span.start + span.len])) {
                span.len += 1;
            }

            if (!memcmp("define", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = Define};
            } else if (!memcmp("include", input + span.start + 1,
                               span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = Include};
            } else if (!memcmp("if", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = If};
            } else if (!memcmp("ifdef", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = IfDefined};
            } else if (!memcmp("ifndef", input + span.start + 1,
                               span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = IfNotDefined};
            } else if (!memcmp("else", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = Else};
            } else if (!memcmp("elif", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = ElseIf};
            } else if (!memcmp("elifdef", input + span.start + 1,
                               span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = ElseIfDefined};
            } else if (!memcmp("elifndef", input + span.start + 1,
                               span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = ElseIfNotDefined};
            } else if (!memcmp("endif", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = EndIf};
            } else if (!memcmp("line", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = Line};
            } else if (!memcmp("embed", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = Embed};
            } else if (!memcmp("error", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = Error};
            } else if (!memcmp("warning", input + span.start + 1,
                               span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = Warning};
            } else if (!memcmp("pragma", input + span.start + 1,
                               span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = Pragma};
            } else if (!memcmp("undef", input + span.start + 1, span.len - 1)) {
                return (Lex){.type = MacroToken,
                             .span = {span.start, span.len},
                             .macro = Undefine};
            }
        } else if (input[idx + 1] == '#') {
            return (Lex){.type = HashHash, .span = {.start = idx, .len = 2}};
        }
    }
    return (Lex){.type = Hash, .span = {.start = idx, .len = 1}};
}

Lex punctuator(const char *input, size_t idx, size_t limit, Vector **id_table) {
    switch (input[idx]) {
    case '[':
        return (Lex){.type = LBracket, .span = {.start = idx, .len = 1}};
    case ']':
        return (Lex){.type = RBracket, .span = {.start = idx, .len = 1}};
    case '(':
        return (Lex){.type = LParen, .span = {.start = idx, .len = 1}};
    case ')':
        return (Lex){.type = RParen, .span = {.start = idx, .len = 1}};
    case '{':
        return (Lex){.type = LSquigly, .span = {.start = idx, .len = 1}};
    case '}':
        return (Lex){.type = RSquigly, .span = {.start = idx, .len = 1}};
    case '.':
        if (input[idx + 1] == '.' && input[idx + 2] == '.') {
            return (Lex){.type = DotDotDot, .span = {.start = idx, .len = 3}};
        }
        return (Lex){.type = Dot, .span = {.start = idx, .len = 1}};
    case '&':
        if (input[idx + 1] == '&')
            return (Lex){.type = EtEt, .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == '=')
            return (Lex){.type = EtEqual, .span = {.start = idx, .len = 2}};
        return (Lex){.type = Et, .span = {.start = idx, .len = 1}};
    case '*':
        if (input[idx + 1] == '=') {
            return (Lex){.type = StarEqual, .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Star, .span = {.start = idx, .len = 1}};
    case '+':
        if (input[idx + 1] == '+')
            return (Lex){.type = PlusPlus, .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == '=')
            return (Lex){.type = PlusEqual, .span = {.start = idx, .len = 2}};
        return (Lex){.type = Plus, .span = {.start = idx, .len = 1}};
    case '-':
        if (input[idx + 1] == '>')
            return (Lex){.type = Arrow, .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == '-')
            return (Lex){.type = MinusMinus, .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == '=')
            return (Lex){.type = MinusEqual, .span = {.start = idx, .len = 2}};
        return (Lex){.type = Minus, .span = {.start = idx, .len = 1}};
    case '~':
        return (Lex){.type = Tilde, .span = {.start = idx, .len = 1}};
    case '!':
        if (input[idx + 1] == '=') {
            return (Lex){.type = ExclamationEqual,
                         .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Exclamation, .span = {.start = idx, .len = 1}};
    case '/':
        if (input[idx + 1] == '=') {
            return (Lex){.type = SlashEqual, .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Slash, .span = {.start = idx, .len = 1}};
    case '%':
        if (input[idx + 1] == ':' &&
            ((input[idx + 2] == '%' && input[idx + 3] == ':') ||
             input[idx + 2] == '#')) {
            return (Lex){.type = HashHash, .span = {.start = idx, .len = 4}};
        } else if (input[idx + 1] == '=')
            return (Lex){.type = PercentEqual,
                         .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == '>')
            return (Lex){.type = RSquigly, .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == ':')
            return macro(input, idx, limit, id_table);
        return (Lex){.type = Percent, .span = {.start = idx, .len = 1}};
    case '<':
        if (input[idx + 1] == '<') {
            if (input[idx + 2] == '=')
                return (Lex){.type = LeftLeftEqual,
                             .span = {.start = idx, .len = 3}};
            return (Lex){.type = LeftLeft, .span = {.start = idx, .len = 2}};
        } else if (input[idx + 1] == '=')
            return (Lex){.type = LeftEqual, .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == ':')
            return (Lex){.type = LBracket, .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == '%')
            return (Lex){.type = LSquigly, .span = {.start = idx, .len = 2}};
        return (Lex){.type = Left, .span = {.start = idx, .len = 1}};
    case '>':
        if (input[idx + 1] == '>') {
            if (input[idx + 2] == '=')
                return (Lex){.type = RightRightEqual,
                             .span = {.start = idx, .len = 3}};
            return (Lex){.type = RightRight, .span = {.start = idx, .len = 2}};
        } else if (input[idx + 1] == '=')
            return (Lex){.type = RightEqual, .span = {.start = idx, .len = 2}};
        return (Lex){.type = Right, .span = {.start = idx, .len = 1}};
    case '^':
        if (input[idx + 1] == '=') {
            return (Lex){.type = CaretEqual, .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Caret, .span = {.start = idx, .len = 1}};
    case '|':
        if (input[idx + 1] == '|')
            return (Lex){.type = PipePipe, .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == '=')
            return (Lex){.type = PipeEqual, .span = {.start = idx, .len = 2}};
        return (Lex){.type = Pipe, .span = {.start = idx, .len = 1}};
    case '?':
        return (Lex){.type = Question, .span = {.start = idx, .len = 1}};
    case ':':
        if (input[idx + 1] == ':')
            return (Lex){.type = ColonColon, .span = {.start = idx, .len = 2}};
        else if (input[idx + 1] == '>')
            return (Lex){.type = RBracket, .span = {.start = idx, .len = 2}};
        return (Lex){.type = Colon, .span = {.start = idx, .len = 1}};
    case ';':
        return (Lex){.type = Semicolon, .span = {.start = idx, .len = 1}};
    case '=':
        if (input[idx + 1] == '=') {
            return (Lex){.type = EqualEqual, .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Equal, .span = {.start = idx, .len = 1}};
    case ',':
        return (Lex){.type = Comma, .span = {.start = idx, .len = 1}};
    case '#':
        return macro(input, idx, limit, id_table);
    default:
        return (Lex){0};
    }
}

// TODO: There is also multibyte chars
Lex constant_char(const char *input, size_t idx, enum lex_type char_lex) {
    size_t offset;
    switch (char_lex) {
    case ConstantChar:
        offset = 0;
        break;
    case ConstantCharU8:
        offset = 2;
        break;
    case ConstantCharU16:
    case ConstantCharU32:
    case ConstantCharWide:
        offset = 1;
        break;
    default:
        return (Lex){.type = Invalid, .span = {idx, 1}, .invalid = IllegalChar};
    }

    if (input[idx + 1] == '\\') {
        char c;
        switch (input[idx + 2]) {
        case '\'':
            c = 0x27;
            goto Single_char;
        case '"':
            c = 0x22;
            goto Single_char;
        case '?':
            c = 0x3f;
            goto Single_char;
        case '\\':
            c = 0x5c;
            goto Single_char;
        case 'a':
            c = 0x07;
            goto Single_char;
        case 'b':
            c = 0x08;
            goto Single_char;
        case 'f':
            c = 0x0c;
            goto Single_char;
        case 'n':
            c = 0x0a;
            goto Single_char;
        case 'r':
            c = 0x0d;
            goto Single_char;
        case 't':
            c = 0x09;
            goto Single_char;
        case 'v':
            c = 0x0b;
            goto Single_char;
        case 'x':
            // TODO: Arbitrary Hex?
            break;
        case 'u':
        case 'U':
            // TODO: Handle Universal character name
            break;
        default:
            if (oct_digit(input[idx + 2])) {
                if (oct_digit(input[idx + 3])) {
                    if (oct_digit(input[idx + 4]) && input[idx + 5] == '\'') {
                        return (Lex){.type = char_lex,
                                     .span = {idx - offset, 6 + offset},
                                     .constant = (input[idx + 2] - '0') * 64 +
                                                 (input[idx + 3] - '0') * 8 +
                                                 (input[idx + 4] - '0')};
                    } else if (input[idx + 4] == '\'') {
                        return (Lex){.type = char_lex,
                                     .span = {idx - offset, 5 + offset},
                                     .constant = (input[idx + 2] - '0') * 8 +
                                                 (input[idx + 3] - '0')};
                    }
                    return (Lex){.type = Invalid,
                                 .span = {idx - offset, 4 + offset},
                                 .invalid = IllegalEscapeChar};
                } else if (input[idx + 3] == '\'') {
                    return (Lex){.type = char_lex,
                                 .span = {idx - offset, 4 + offset},
                                 .constant = input[idx + 2] - '0'};
                }
            }
        }
        return (Lex){.type = Invalid,
                     .span = {idx - offset, 2 + offset},
                     .invalid = IllegalEscapeChar};
    Single_char:
        if (input[idx + 3] == '\'') {
            return (Lex){.type = char_lex,
                         .span = {idx - offset, 4 + offset},
                         .constant = c};
        }
        return (Lex){.type = Invalid,
                     .span = {idx - offset, 3 + offset},
                     .invalid = UnfinishedChar};
    }

    if (input[idx + 2] == '\'') {
        return (Lex){.type = char_lex,
                     .span = {idx - offset, 3 + offset},
                     .constant = input[idx + 1]};
    }

    return (Lex){.type = Invalid,
                 .span = {idx - offset, 2 + offset},
                 .invalid = UnfinishedChar};
}

enum lex_type get_integer_suffix(const char *input, Span *span) {
    switch (input[span->start + span->len]) {
    case 'u':
    case 'U':
        switch (input[span->start + span->len + 1]) {
        case 'l':
            if (input[span->start + span->len + 2] == 'l') {
                span->len += 3;
                return ConstantUnsignedLongLong;
            } else {
                span->len += 2;
                return ConstantUnsignedLong;
            }
        case 'L':
            if (input[span->start + span->len + 2] == 'L') {
                span->len += 3;
                return ConstantUnsignedLongLong;
            } else {
                span->len += 2;
                return ConstantUnsignedLong;
            }
        case 'w':
        case 'W':
            span->len += 2;
            return ConstantUnsignedBitPrecise;
        default:
            span->len += 1;
            return ConstantUnsigned;
        }
    case 'l':
        if (input[span->start + span->len + 1] == 'l') {
            if (input[span->start + span->len + 2] == 'u' ||
                input[span->start + span->len + 2] == 'U') {
                span->len += 3;
                return ConstantUnsignedLongLong;
            }
            span->len += 2;
            return ConstantLongLong;
        } else if (input[span->start + span->len + 1] == 'u' ||
                   input[span->start + span->len + 1] == 'U') {
            span->len += 2;
            return ConstantUnsignedLong;
        }
        span->len += 1;
        return ConstantLong;
    case 'L':
        if (input[span->start + span->len + 1] == 'L') {
            if (input[span->start + span->len + 2] == 'u' ||
                input[span->start + span->len + 2] == 'U') {
                span->len += 3;
                return ConstantUnsignedLongLong;
            }
            span->len += 2;
            return ConstantLongLong;
        } else if (input[span->start + span->len + 1] == 'u' ||
                   input[span->start + span->len + 1] == 'U') {
            span->len += 2;
            return ConstantUnsignedLong;
        }
        span->len += 1;
        return ConstantLong;
    case 'w':
    case 'W':
        if (input[span->start + span->len + 1] == 'u' ||
            input[span->start + span->len + 1] == 'U') {
            span->len += 2;
            return ConstantUnsignedBitPrecise;
        }
        span->len += 1;
        return ConstantBitPrecise;
    default:
        return Constant;
    }
}

enum lex_type get_float_suffix(const char *input, Span *span) {
    switch (input[span->start + span->len]) {
    case 'f':
    case 'F':
        span->len += 1;
        return ConstantFloat;
    case 'l':
    case 'L':
        span->len += 1;
        return ConstantLongDouble;
    case 'd':
        switch (input[span->start + span->len + 1]) {
        case 'f':
            span->len += 2;
            return ConstantDecimal32;
        case 'd':
            span->len += 2;
            return ConstantDecimal64;
        case 'l':
            span->len += 2;
            return ConstantDecimal128;
        }
    case 'D':
        switch (input[span->start + span->len + 1]) {
        case 'F':
            span->len += 2;
            return ConstantDecimal32;
        case 'D':
            span->len += 2;
            return ConstantDecimal64;
        case 'L':
            span->len += 2;
            return ConstantDecimal128;
        }
    default:
        return ConstantDouble;
    }
}

// TODO: Proper float conversion
Lex dec_float_constant(const char *input, uint64_t root, Span span) {
    int64_t root_pow = 0;

    while (digit(input[span.start + span.len])) {
        root *= 10;
        root += input[span.start + span.len] - '0';
        root_pow -= 1;

        span.len += 1;
        if (input[span.start + span.len] == '\'' &&
            digit(input[span.start + span.len + 1])) {
            span.len += 1;
        }
    }

    int64_t exp = 0;
    if (input[span.start + span.len] == 'e' ||
        input[span.start + span.len] == 'E') {
        span.len += 1;
        int64_t sign = 1;

        if (input[span.start + span.len] == '+') {
            span.len += 1;
        } else if (input[span.start + span.len] == '-') {
            span.len += 1;
            sign = -1;
        }

        if (!digit(input[span.start + span.len])) {
            return (Lex){
                .type = Invalid, .span = span, .invalid = IllegalFloat};
        }

        while (digit(input[span.start + span.len])) {
            exp *= 10;
            exp += input[span.start + span.len] - '0';

            span.len += 1;
            if (input[span.start + span.len] == '\'' &&
                digit(input[span.start + span.len + 1])) {
                span.len += 1;
            }
        }

        exp *= sign;
    }

    enum lex_type suffixed = get_float_suffix(input, &span);

    return (Lex){.type = suffixed,
                 .span = span,
                 .floating = ((long double)root) * pow(10, exp + root_pow)};
}

Lex dec_or_float_constant(const char *input, Span span) {
    uint64_t constant = input[span.start + span.len] - '0';
    span.len += 1;

    while (digit(input[span.start + span.len])) {
        constant *= 10;
        constant += input[span.start + span.len] - '0';

        span.len += 1;
        if (input[span.start + span.len] == '\'' &&
            digit(input[span.start + span.len + 1])) {
            span.len += 1;
        }
    }

    if (input[span.start + span.len] == '.') {
        span.len += 1;
        return dec_float_constant(input, constant, span);
    } else if (input[span.start + span.len] == 'e' ||
               input[span.start + span.len] == 'E') {
        return dec_float_constant(input, constant, span);
    }

    enum lex_type suffixed = get_integer_suffix(input, &span);

    return (Lex){.type = suffixed, .span = span, .constant = constant};
}

// TODO: This needs a proper implementation
// NOTE: Exponent is mandatory for hex but not dec
Lex hex_float_constant(const char *input, uint64_t root, Span span) {
    int64_t root_pow = 0;

    while (hex_digit(input[span.start + span.len])) {
        root *= 16;
        root += hex_acquire(input[span.start + span.len]);
        root_pow -= 1;

        span.len += 1;
        if (input[span.start + span.len] == '\'' &&
            hex_digit(input[span.start + span.len + 1])) {
            span.len += 1;
        }
    }

    int64_t exp = 0;
    if (input[span.start + span.len] == 'p' ||
        input[span.start + span.len] == 'P') {
        span.len += 1;
        int64_t sign = 1;

        if (input[span.start + span.len] == '+') {
            span.len += 1;
        } else if (input[span.start + span.len] == '-') {
            span.len += 1;
            sign = -1;
        }

        if (!hex_digit(input[span.start + span.len])) {
            return (Lex){
                .type = Invalid, .span = span, .invalid = IllegalFloat};
        }

        while (hex_digit(input[span.start + span.len])) {
            exp *= 16;
            exp += hex_acquire(input[span.start + span.len]);

            span.len += 1;
            if (input[span.start + span.len] == '\'' &&
                hex_digit(input[span.start + span.len + 1])) {
                span.len += 1;
            }
        }

        exp *= sign;
    } else {
        return (Lex){
            .type = Invalid, .span = span, .invalid = IllegalFloatHexExponent};
    }

    enum lex_type suffixed = get_float_suffix(input, &span);
    if (suffixed == ConstantDecimal32 || suffixed == ConstantDecimal64 ||
        suffixed == ConstantDecimal128) {
        return (Lex){
            .type = Invalid, .span = span, .invalid = IllegalFloatHexSuffix};
    }

    return (Lex){.type = suffixed,
                 .span = span,
                 .floating =
                     ((long double)root) * ((exp + root_pow >= 0)
                                                ? 2 << (exp + root_pow * 4)
                                                : 2 >> (exp + root_pow * 4))};
}

Lex hex_or_float_constant(const char *input, Span span) {
    uint64_t constant = hex_acquire(input[span.start + span.len]);
    span.len += 1;

    while (hex_digit(input[span.start + span.len])) {
        constant *= 16;
        constant += hex_acquire(input[span.start + span.len]);

        span.len += 1;
        if (input[span.start + span.len] == '\'' &&
            hex_digit(input[span.start + span.len + 1])) {
            span.len += 1;
        }
    }
    if (input[span.start + span.len] == '.') {
        span.len += 1;
        return dec_float_constant(input, constant, span);
    } else if (input[span.start + span.len] == 'p' ||
               input[span.start + span.len] == 'P') {
        return dec_float_constant(input, constant, span);
    }

    enum lex_type suffixed = get_integer_suffix(input, &span);

    return (Lex){.type = suffixed, .span = span, .constant = constant};
}

Lex num_constant(const char *input, size_t idx, Span span, uint64_t base,
                 int (*xdigit)(char c), uint64_t (*xacquire)(char c)) {
    uint64_t constant = xacquire(input[idx]);

    while (xdigit(input[span.start + span.len])) {
        constant *= base;
        constant += xacquire(input[span.start + span.len]);

        span.len += 1;
        if (input[span.start + span.len] == '\'' &&
            xdigit(input[span.start + span.len + 1])) {
            span.len += 1;
        }
    }

    enum lex_type suffixed = get_integer_suffix(input, &span);

    return (Lex){.type = suffixed, .span = span, .constant = constant};
}

Lex constant(const char *input, size_t idx) {
    Lex result = (Lex){0};
    switch (input[idx]) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        result = dec_or_float_constant(input, (Span){idx, 0});
        break;
    case '.':
        if (digit(input[idx + 1])) {
            result = dec_float_constant(input, 0, (Span){idx, 1});
        }
        break;
    case '0':
        if ((input[idx + 1] == 'x' || input[idx + 1] == 'X')) {
            if (hex_digit(input[idx + 2])) {
                result = hex_or_float_constant(input, (Span){idx, 2});
            } else if (input[idx + 2] == '.') {
                result = hex_float_constant(input, 0, (Span){idx, 3});
            }
        } else if ((input[idx + 1] == 'b' || input[idx + 1] == 'B') &&
                   bin_digit(input[idx + 2])) {
            result = num_constant(input, idx + 2, (Span){idx, 3}, 2, bin_digit,
                                  acquire);
        } else if (oct_digit(input[idx + 1])) {
            result = num_constant(input, idx + 1, (Span){idx, 2}, 8, oct_digit,
                                  acquire);
        } else {
            Span span = {idx, 1};
            enum lex_type suffixed = get_integer_suffix(input, &span);
            result = (Lex){.type = suffixed, .span = span, .constant = 0};
        }
        break;
    case '\'':
        result = constant_char(input, idx, ConstantChar);
        break;
        // TODO: These need more proper handling
    case 'u':
        if (input[idx + 1] == '8' && input[idx + 2] == '\'') {
            result = constant_char(input, idx + 2, ConstantCharU8);
        } else if (input[idx + 1] == '\'') {
            result = constant_char(input, idx + 1, ConstantCharU16);
        }
        break;
    case 'U':
        if (input[idx + 1] == '\'') {
            result = constant_char(input, idx + 1, ConstantCharU32);
        }
        break;
    case 'L':
        if (input[idx + 1] == '\'') {
            result = constant_char(input, idx + 1, ConstantCharWide);
        }
        break;
    default:
        break;
    }

    return result;
}

Lex string_ret(const char *input, size_t idx, size_t limit,
               enum lex_type str_lex, Vector **id_table) {
    Span span = {idx, 1};
    size_t offset;
    switch (str_lex) {
    case StringU8:
        offset = 2;
        break;
    case StringU16:
    case StringU32:
    case StringWide:
        offset = 1;
        break;
    case String:
        offset = 0;
        break;
    default:
        return (Lex){.type = Invalid, .span = span, .invalid = IllegalString};
    }

    while (input[span.start + span.len] != '"') {
        if (input[span.start + span.len] == '\\' &&
            input[span.start + span.len + 1] == '"') {
            span.len += 2;
        } else {
            span.len += 1;
        }

        if (limit <= span.len) {
            return (Lex){.type = Invalid,
                         .span = {span.start - offset, span.len + offset},
                         .invalid = IllegalString};
        }
    }

    span.len += 1;

    return (Lex){.type = str_lex,
                 .span = {span.start - offset, span.len + offset},
                 .id = search_id_table(input, span, id_table)};
}

Lex string(const char *input, size_t idx, size_t limit, Vector **id_table) {
    if (input[idx] == '"') {
        return string_ret(input, idx, limit, String, id_table);
    } else if (input[idx] == 'u') {
        if (input[idx + 1] == '8' && input[idx + 2] == '"') {
            return string_ret(input, idx + 2, limit - 2, StringU8, id_table);
        } else if (input[idx + 1] == '"') {
            return string_ret(input, idx + 1, limit - 1, StringU16, id_table);
        }
    } else if (input[idx] == 'U' && input[idx + 1] == '"') {
        return string_ret(input, idx + 1, limit - 1, StringU32, id_table);
    } else if (input[idx] == 'L' && input[idx + 1] == '"') {
        return string_ret(input, idx + 1, limit - 1, StringWide, id_table);
    }

    return (Lex){0};
}

Lex comment(const char *input, size_t idx, size_t limit) {
    if (input[idx] == '/') {
        if (input[idx + 1] == '/') {
            Span span = {idx, 2};
            while (input[span.start + span.len] != '\n') {
                span.len += 1;
                if (limit <= span.len) {
                    return (Lex){.type = Eof};
                }
            }
            return (Lex){.type = Comment, .span = span};
        } else if (input[idx + 1] == '*') {
            Span span = {idx, 2};
            while (input[span.start + span.len] != '*' ||
                   input[span.start + span.len + 1] != '/') {
                span.len += 1;
                if (limit <= span.len + 1) {
                    return (Lex){.type = Eof};
                }
            }

            span.len += 2;

            return (Lex){.type = Comment, .span = span};
        }
    }

    return (Lex){0};
}

Lex lex_next(const char *input, size_t len, size_t *idx, Ids **id_table) {
    Lex lexed;
    while (*idx < len) {
        lexed = keyword_or_id(input, *idx, id_table);
        if (lexed.type || lexed.invalid) {
            *idx += lexed.span.len;
            return lexed;
        }

        lexed = constant(input, *idx);
        if (lexed.type || lexed.invalid) {
            *idx += lexed.span.len;
            return lexed;
        }

        lexed = string(input, *idx, len - *idx, id_table);
        if (lexed.type || lexed.invalid) {
            *idx += lexed.span.len;
            return lexed;
        }

        lexed = comment(input, *idx, len - *idx);
        if (lexed.type) {
            if (lexed.type == Eof) {
                return lexed;
            } else {
                *idx += lexed.span.len;
                continue;
            }
        }

        lexed = punctuator(input, *idx, len - *idx, id_table);
        if (lexed.type || lexed.invalid) {
            *idx += lexed.span.len;
            return lexed;
        }

        if (!space(input[*idx])) {
            *idx += 1;
            return (Lex){
                .type = Invalid, .span = {*idx - 1, 1}, .invalid = IllegalChar};
        }
        *idx += 1;
    }

    return (Lex){.type = Eof};
}

void print_ids(const char *input, const Ids *ids) {
    for (uint64_t i = 0; i < ids->length; i++) {
        Span span = ((Span *)ids->v)[i];
        printf("<Id %zu %.*s>\n", i, (int)span.len, input + span.start);
    }
}

void print_lexes(const Lexes *lexes) {
    for (uint64_t i = 0; i < lexes->length; i++) {
        Lex lex = ((Lex *)lexes->v)[i];
        switch (lex.type) {
        case Invalid:
            printf(":Lex Error %d: [%zu, %zu]\n", lex.invalid, lex.span.start,
                   lex.span.len);
            break;
        case Eof:
            printf(":End of File: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case StringU8:
            printf(":StringU8 id %zu: [%zu, %zu]\n", lex.id, lex.span.start,
                   lex.span.len);
            break;
        case StringU16:
            printf(":StringU16 id %zu: [%zu, %zu]\n", lex.id, lex.span.start,
                   lex.span.len);
            break;
        case StringU32:
            printf(":StringU32 id %zu: [%zu, %zu]\n", lex.id, lex.span.start,
                   lex.span.len);
            break;
        case StringWide:
            printf(":StringWide id %zu: [%zu, %zu]\n", lex.id, lex.span.start,
                   lex.span.len);
            break;
        case String:
            printf(":String id %zu: [%zu, %zu]\n", lex.id, lex.span.start,
                   lex.span.len);
            break;
        case Identifier:
            printf(":Identifier id %zu: [%zu, %zu]\n", lex.id, lex.span.start,
                   lex.span.len);
            break;
        case ConstantUnsignedLongLong:
            printf(":UnsidgnedLongLongNumber %zu: [%zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len);
            break;
        case ConstantUnsignedLong:
            printf(":UnsidgnedLongNumber %zu: [%zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len);
            break;
        case ConstantUnsignedBitPrecise:
            printf(":UnsidgnedBitPreciseNumber %zu: [%zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len);
            break;
        case ConstantUnsigned:
            printf(":UnsidgnedNumber %zu: [%zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len);
            break;
        case ConstantLongLong:
            printf(":LongLongNumber %zu: [%zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len);
            break;
        case ConstantLong:
            printf(":LongNumber %zu: [%zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len);
            break;
        case ConstantBitPrecise:
            printf(":BitPreciseNumber %zu: [%zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len);
            break;
        case Constant:
            printf(":Number %zu: [%zu, %zu]\n", lex.constant, lex.span.start,
                   lex.span.len);
            break;
        case ConstantFloat:
            printf(":Float %f: [%zu, %zu]\n", lex.floating, lex.span.start,
                   lex.span.len);
            break;
        case ConstantDouble:
            printf(":Double %f: [%zu, %zu]\n", lex.floating, lex.span.start,
                   lex.span.len);
            break;
        case ConstantLongDouble:
            printf(":LongDouble %f: [%zu, %zu]\n", lex.floating, lex.span.start,
                   lex.span.len);
            break;
        case ConstantDecimal32:
            printf(":Dec32 %f: [%zu, %zu]\n", lex.floating, lex.span.start,
                   lex.span.len);
            break;
        case ConstantDecimal64:
            printf(":Dec64 %f: [%zu, %zu]\n", lex.floating, lex.span.start,
                   lex.span.len);
            break;
        case ConstantDecimal128:
            printf(":Dec128 %f: [%zu, %zu]\n", lex.floating, lex.span.start,
                   lex.span.len);
            break;
        case ConstantCharU8:
            printf(":CharU8 %zu: [%zu, %zu]\n", lex.constant, lex.span.start,
                   lex.span.len);
            break;
        case ConstantCharU16:
            printf(":CharU16 %zu: [%zu, %zu]\n", lex.constant, lex.span.start,
                   lex.span.len);
            break;
        case ConstantCharU32:
            printf(":CharU32 %zu: [%zu, %zu]\n", lex.constant, lex.span.start,
                   lex.span.len);
            break;
        case ConstantCharWide:
            printf(":CharWide %zu: [%zu, %zu]\n", lex.constant, lex.span.start,
                   lex.span.len);
            break;
        case ConstantChar:
            printf(":Char %zu: [%zu, %zu]\n", lex.constant, lex.span.start,
                   lex.span.len);
            break;
        case Keyword:
            switch (lex.key) {
            case KEY_alignas:
                printf(":Keyword alignas: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_alignof:
                printf(":Keyword alignof: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_auto:
                printf(":Keyword auto: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_bool:
                printf(":Keyword bool: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_break:
                printf(":Keyword break: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_case:
                printf(":Keyword case: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_char:
                printf(":Keyword char: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_const:
                printf(":Keyword const: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_constexpr:
                printf(":Keyword constexpr: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_continue:
                printf(":Keyword continue: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_default:
                printf(":Keyword default: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_do:
                printf(":Keyword do: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_double:
                printf(":Keyword double: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_else:
                printf(":Keyword else: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_enum:
                printf(":Keyword enum: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_extern:
                printf(":Keyword extern: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_false:
                printf(":Keyword false: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_float:
                printf(":Keyword float: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_for:
                printf(":Keyword for: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_goto:
                printf(":Keyword goto: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_if:
                printf(":Keyword if: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_inline:
                printf(":Keyword inline: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_int:
                printf(":Keyword int: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_long:
                printf(":Keyword long: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_nullptr:
                printf(":Keyword nullptr: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_register:
                printf(":Keyword register: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_restrict:
                printf(":Keyword restrict: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_return:
                printf(":Keyword return: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_short:
                printf(":Keyword short: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_signed:
                printf(":Keyword signed: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_sizeof:
                printf(":Keyword sizeof: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_static:
                printf(":Keyword static: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_static_assert:
                printf(":Keyword static: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_struct:
                printf(":Keyword struct: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_switch:
                printf(":Keyword switch: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_thread_local:
                printf(":Keyword thread: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_true:
                printf(":Keyword true: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_typedef:
                printf(":Keyword typedef: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_typeof:
                printf(":Keyword typeof: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_typeof_unqual:
                printf(":Keyword typeof: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_union:
                printf(":Keyword union: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_unsigned:
                printf(":Keyword unsigned: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_void:
                printf(":Keyword void: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_volatile:
                printf(":Keyword volatile: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_while:
                printf(":Keyword while: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY__Atomic:
                printf(":Keyword _Atomic: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY__BitInt:
                printf(":Keyword _BitInt: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY__Complex:
                printf(":Keyword _Complex: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY__Decimal128:
                printf(":Keyword _Decimal128: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY__Decimal32:
                printf(":Keyword _Decimal32: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY__Decimal64:
                printf(":Keyword _Decimal64: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY__Generic:
                printf(":Keyword _Generic: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY__Imaginary:
                printf(":Keyword _Imaginary: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY__Noreturn:
                printf(":Keyword _Noreturn: [%zu, %zu]\n", lex.span.start,
                       lex.span.len);
                break;
            case KEY_COUNT:
                break;
            }
            break;
        case MacroToken:
            printf(":MacroToken: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case LBracket:
            printf(":LBracket: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case RBracket:
            printf(":RBracket: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case LParen:
            printf(":LParen: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case RParen:
            printf(":RParen: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case LSquigly:
            printf(":LSquigly: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case RSquigly:
            printf(":RSquigly: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Dot:
            printf(":Dot: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Arrow:
            printf(":Arrow: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case PlusPlus:
            printf(":PlusPlus: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case MinusMinus:
            printf(":MinusMinus: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Et:
            printf(":Et: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Star:
            printf(":Star: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Plus:
            printf(":Plus: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Minus:
            printf(":Minus: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Tilde:
            printf(":Tilde: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Exclamation:
            printf(":Exclamation: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Slash:
            printf(":Slash: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Percent:
            printf(":Percent: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case LeftLeft:
            printf(":LeftLeft: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case RightRight:
            printf(":RightRight: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Left:
            printf(":Left: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Right:
            printf(":Right: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case LeftEqual:
            printf(":LeftEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case RightEqual:
            printf(":RightEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case EqualEqual:
            printf(":EqualEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case ExclamationEqual:
            printf(":ExclamationEqual: [%zu, %zu]\n", lex.span.start,
                   lex.span.len);
            break;
        case Caret:
            printf(":Caret: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Pipe:
            printf(":Pipe: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case EtEt:
            printf(":EtEt: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case PipePipe:
            printf(":PipePipe: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Question:
            printf(":Question: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Colon:
            printf(":Colon: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case ColonColon:
            printf(":ColonColon: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Semicolon:
            printf(":Semicolon: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case DotDotDot:
            printf(":DotDotDot: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Equal:
            printf(":Equal: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case StarEqual:
            printf(":StarEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case SlashEqual:
            printf(":SlashEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case PercentEqual:
            printf(":PercentEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case PlusEqual:
            printf(":PlusEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case MinusEqual:
            printf(":MinusEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case LeftLeftEqual:
            printf(":LeftLeftEqual: [%zu, %zu]\n", lex.span.start,
                   lex.span.len);
            break;
        case RightRightEqual:
            printf(":RightRightEqual: [%zu, %zu]\n", lex.span.start,
                   lex.span.len);
            break;
        case EtEqual:
            printf(":EtEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case CaretEqual:
            printf(":CaretEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case PipeEqual:
            printf(":PipeEqual: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Comma:
            printf(":Comma: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Hash:
            printf(":Hash: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case HashHash:
            printf(":HashHash: [%zu, %zu]\n", lex.span.start, lex.span.len);
            break;
        case Comment:
            break;
        }
    }
}
