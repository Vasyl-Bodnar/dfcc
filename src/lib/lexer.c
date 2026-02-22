/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lexer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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
size_t search_id_table(const Span span, Ids **id_table) {
    for (size_t i = 0; i < (*id_table)->length; i++) {
        Span id = *(Span *)at_elem_vec(*id_table, i);
        if (span.len == id.len && !memcmp(id.start, span.start, span.len)) {
            return i;
        }
    }
    push_elem_vec(id_table, &span);
    return (*id_table)->length - 1;
}

Lex check_keyword(const Span span) {
    if (span.len < 2 || span.len > 13) {
        return (Lex){0};
    }
    switch (*span.start) {
    case 'a':
        if (span.len == key_len_table[KEY_auto] &&
            !memcmp("auto", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_auto};
        } else if (span.len == key_len_table[KEY_alignas] &&
                   !memcmp("alignas", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_alignas};
        } else if (span.len == key_len_table[KEY_alignof] &&
                   !memcmp("alignof", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_alignof};
        }
        break;
    case 'b':
        if (span.len == key_len_table[KEY_break] &&
            !memcmp("break", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_break};
        } else if (span.len == key_len_table[KEY_bool] &&
                   !memcmp("bool", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_bool};
        }
        break;
    case 'c':
        if (span.len < 4 || span.len > 9) {
            break;
        } else if (span.len == key_len_table[KEY_continue] &&
                   !memcmp("continue", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY_continue};
        } else if (span.len == key_len_table[KEY_constexpr] &&
                   !memcmp("constexpr", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY_constexpr};
        } else if (span.len == key_len_table[KEY_const] &&
                   !memcmp("const", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_const};
        } else if (span.len == key_len_table[KEY_char] &&
                   !memcmp("char", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_char};
        } else if (span.len == key_len_table[KEY_case] &&
                   !memcmp("case", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_case};
        }
        break;
    case 'd':
        if (span.len == key_len_table[KEY_do] &&
            !memcmp("do", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_do};
        } else if (span.len == key_len_table[KEY_double] &&
                   !memcmp("double", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_double};
        } else if (span.len == key_len_table[KEY_default] &&
                   !memcmp("default", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_default};
        }
        break;
    case 'e':
        if (span.len == key_len_table[KEY_extern] &&
            !memcmp("extern", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_extern};
        } else if (span.len == key_len_table[KEY_enum] &&
                   !memcmp("enum", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_enum};
        } else if (span.len == key_len_table[KEY_else] &&
                   !memcmp("else", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_else};
        }
        break;
    case 'f':
        if (span.len == key_len_table[KEY_for] &&
            !memcmp("for", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_for};
        } else if (span.len == key_len_table[KEY_float] &&
                   !memcmp("float", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_float};
        } else if (span.len == key_len_table[KEY_false] &&
                   !memcmp("false", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_false};
        }
        break;
    case 'g':
        if (span.len == key_len_table[KEY_goto] &&
            !memcmp("goto", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_goto};
        }
        break;
    case 'i':
        if (span.len == key_len_table[KEY_int] &&
            !memcmp("int", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_int};
        } else if (span.len == key_len_table[KEY_inline] &&
                   !memcmp("inline", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_inline};
        } else if (span.len == key_len_table[KEY_if] &&
                   !memcmp("if", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_if};
        }
        break;
    case 'l':
        if (span.len == key_len_table[KEY_long] &&
            !memcmp("long", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_long};
        }
        break;
    case 'n':
        if (span.len == key_len_table[KEY_nullptr] &&
            !memcmp("nullptr", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_nullptr};
        }
        break;
    case 'r':
        if (span.len == key_len_table[KEY_return] &&
            !memcmp("return", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_return};
        } else if (span.len == key_len_table[KEY_restrict] &&
                   !memcmp("restrict", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY_restrict};
        } else if (span.len == key_len_table[KEY_register] &&
                   !memcmp("register", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY_register};
        }
        break;
    case 's':
        if (span.len < 5) {
            break;
        } else if (span.len == key_len_table[KEY_switch] &&
                   !memcmp("switch", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_switch};
        } else if (span.len == key_len_table[KEY_struct] &&
                   !memcmp("struct", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_struct};
        } else if (span.len == key_len_table[KEY_static_assert] &&
                   !memcmp("static_assert", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY_static_assert};
        } else if (span.len == key_len_table[KEY_static] &&
                   !memcmp("static", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_static};
        } else if (span.len == key_len_table[KEY_sizeof] &&
                   !memcmp("sizeof", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_sizeof};
        } else if (span.len == key_len_table[KEY_signed] &&
                   !memcmp("signed", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_signed};
        } else if (span.len == key_len_table[KEY_short] &&
                   !memcmp("short", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_short};
        }
        break;
    case 't':
        if (span.len < 4) {
            break;
        } else if (span.len == key_len_table[KEY_typeof_unqual] &&
                   !memcmp("typeof_unqual", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY_typeof_unqual};
        } else if (span.len == key_len_table[KEY_typeof] &&
                   !memcmp("typeof", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_typeof};
        } else if (span.len == key_len_table[KEY_typedef] &&
                   !memcmp("typedef", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_typedef};
        } else if (span.len == key_len_table[KEY_true] &&
                   !memcmp("true", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_true};
        } else if (span.len == key_len_table[KEY_thread_local] &&
                   !memcmp("thread_local", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY_thread_local};
        }
        break;
    case 'u':
        if (span.len == key_len_table[KEY_union] &&
            !memcmp("union", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_union};
        } else if (span.len == key_len_table[KEY_unsigned] &&
                   !memcmp("unsigned", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY_unsigned};
        }
        break;
    case 'v':
        if (span.len == key_len_table[KEY_void] &&
            !memcmp("void", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_void};
        } else if (span.len == key_len_table[KEY_volatile] &&
                   !memcmp("volatile", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY_volatile};
        }
        break;
    case 'w':
        if (span.len == key_len_table[KEY_while] &&
            !memcmp("while", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY_while};
        }
        break;
    case '_':
        if (span.len < 7 || span.len > 11) {
            break;
        } else if (span.len == key_len_table[KEY__Noreturn] &&
                   !memcmp("_Noreturn", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY__Noreturn};
        } else if (span.len == key_len_table[KEY__Imaginary] &&
                   !memcmp("_Imaginary", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY__Imaginary};
        } else if (span.len == key_len_table[KEY__Generic] &&
                   !memcmp("_Generic", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY__Generic};
        } else if (span.len == key_len_table[KEY__Decimal32] &&
                   !memcmp("_Decimal32", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY__Decimal32};
        } else if (span.len == key_len_table[KEY__Decimal64] &&
                   !memcmp("_Decimal64", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY__Decimal64};
        } else if (span.len == key_len_table[KEY__Decimal128] &&
                   !memcmp("_Decimal128", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY__Decimal128};
        } else if (span.len == key_len_table[KEY__Complex] &&
                   !memcmp("_Complex", span.start, span.len)) {
            return (Lex){
                .type = LEX_Keyword, .span = span, .key = KEY__Complex};
        } else if (span.len == key_len_table[KEY__BitInt] &&
                   !memcmp("_BitInt", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY__BitInt};
        } else if (span.len == key_len_table[KEY__Atomic] &&
                   !memcmp("_Atomic", span.start, span.len)) {
            return (Lex){.type = LEX_Keyword, .span = span, .key = KEY__Atomic};
        }
        break;
    default:
        break;
    }
    return (Lex){0};
}

uint32_t check_string_prefix(const Span span) {
    return ((span.len == 2 && *span.start == 'u' && span.start[1] == '8' &&
             (span.start[2] == '\'' || span.start[2] == '"')) ||
            (span.len == 1 &&
             (*span.start == 'u' || *span.start == 'U' || *span.start == 'L') &&
             (span.start[1] == '\'' || span.start[1] == '"')));
}

// MINOR: Possibly handle XID_Start and XID_Continue
Lex keyword_or_id(const Stream *stream, Vector **id_table) {
    if (nondigit(stream->start[stream->idx])) {
        char *input = (char *)stream->start + stream->idx;
        size_t len = 1;

        while (nondigit_or_digit(input[len])) {
            len += 1;
        }

        Span span = {input, len, stream->row, stream->col};
        Lex keyword = check_keyword(span);
        if (keyword.type) {
            return keyword;
        }

        if (check_string_prefix(span)) {
            return (Lex){0};
        }

        return (Lex){.type = LEX_Identifier,
                     .span = span,
                     .id = search_id_table(span, id_table)};
    }
    return (Lex){0};
}

Lex macro(Stream *stream, Vector **id_table) {
    if (stream->len > stream->idx) {
        if (nondigit(stream->start[stream->idx + 1])) {
            char *input = (char *)stream->start + stream->idx;
            size_t len = 2;

            while (nondigit(input[len])) {
                len += 1;
            }

            Span span = {input, len, stream->row, stream->col};
            if (!memcmp("define", span.start + 1, span.len - 1)) {
                stream->macro_line = Define;
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = Define};
            } else if (!memcmp("include", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = Include};
            } else if (!memcmp("if", span.start + 1, span.len - 1)) {
                stream->macro_line = If;
                return (Lex){.type = LEX_MacroToken, .span = span, .macro = If};
            } else if (!memcmp("ifdef", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = IfDefined};
            } else if (!memcmp("ifndef", span.start + 1, span.len - 1)) {
                return (Lex){.type = LEX_MacroToken,
                             .span = span,
                             .macro = IfNotDefined};
            } else if (!memcmp("else", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = Else};
            } else if (!memcmp("elif", span.start + 1, span.len - 1)) {
                stream->macro_line = ElseIf;
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = ElseIf};
            } else if (!memcmp("elifdef", span.start + 1, span.len - 1)) {
                return (Lex){.type = LEX_MacroToken,
                             .span = span,
                             .macro = ElseIfDefined};
            } else if (!memcmp("elifndef", span.start + 1, span.len - 1)) {
                return (Lex){.type = LEX_MacroToken,
                             .span = span,
                             .macro = ElseIfNotDefined};
            } else if (!memcmp("endif", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = EndIf};
            } else if (!memcmp("line", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = Line};
            } else if (!memcmp("embed", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = Embed};
            } else if (!memcmp("error", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = Error};
            } else if (!memcmp("warning", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = Warning};
            } else if (!memcmp("pragma", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = Pragma};
            } else if (!memcmp("undef", span.start + 1, span.len - 1)) {
                return (Lex){
                    .type = LEX_MacroToken, .span = span, .macro = Undefine};
            }
        } else if (stream->start[stream->idx + 1] == '#') {
            return (Lex){.type = LEX_HashHash, .span = from_stream(stream, 2)};
        }
    }
    return (Lex){.type = LEX_Hash, .span = from_stream(stream, 1)};
}

Lex punctuator(Stream *stream, Vector **id_table) {
    switch (stream->start[stream->idx]) {
    case '[':
        return (Lex){.type = LEX_LBracket, .span = from_stream(stream, 1)};
    case ']':
        return (Lex){.type = LEX_RBracket, .span = from_stream(stream, 1)};
    case '(':
        return (Lex){.type = LEX_LParen, .span = from_stream(stream, 1)};
    case ')':
        return (Lex){.type = LEX_RParen, .span = from_stream(stream, 1)};
    case '{':
        return (Lex){.type = LEX_LSquigly, .span = from_stream(stream, 1)};
    case '}':
        return (Lex){.type = LEX_RSquigly, .span = from_stream(stream, 1)};
    case '.':
        if (stream->start[stream->idx + 1] == '.' &&
            stream->start[stream->idx + 2] == '.') {
            return (Lex){.type = LEX_DotDotDot, .span = from_stream(stream, 3)};
        }
        return (Lex){.type = LEX_Dot, .span = from_stream(stream, 1)};
    case '&':
        if (stream->start[stream->idx + 1] == '&')
            return (Lex){.type = LEX_EtEt, .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == '=')
            return (Lex){.type = LEX_EtEqual, .span = from_stream(stream, 2)};
        return (Lex){.type = LEX_Et, .span = from_stream(stream, 1)};
    case '*':
        if (stream->start[stream->idx + 1] == '=') {
            return (Lex){.type = LEX_StarEqual, .span = from_stream(stream, 2)};
        }
        return (Lex){.type = LEX_Star, .span = from_stream(stream, 1)};
    case '+':
        if (stream->start[stream->idx + 1] == '+')
            return (Lex){.type = LEX_PlusPlus, .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == '=')
            return (Lex){.type = LEX_PlusEqual, .span = from_stream(stream, 2)};
        return (Lex){.type = LEX_Plus, .span = from_stream(stream, 1)};
    case '-':
        if (stream->start[stream->idx + 1] == '>')
            return (Lex){.type = LEX_Arrow, .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == '-')
            return (Lex){.type = LEX_MinusMinus,
                         .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == '=')
            return (Lex){.type = LEX_MinusEqual,
                         .span = from_stream(stream, 2)};
        return (Lex){.type = LEX_Minus, .span = from_stream(stream, 1)};
    case '~':
        return (Lex){.type = LEX_Tilde, .span = from_stream(stream, 1)};
    case '!':
        if (stream->start[stream->idx + 1] == '=') {
            return (Lex){.type = LEX_ExclamationEqual,
                         .span = from_stream(stream, 2)};
        }
        return (Lex){.type = LEX_Exclamation, .span = from_stream(stream, 1)};
    case '/':
        if (stream->start[stream->idx + 1] == '=') {
            return (Lex){.type = LEX_SlashEqual,
                         .span = from_stream(stream, 2)};
        }
        return (Lex){.type = LEX_Slash, .span = from_stream(stream, 1)};
    case '%':
        if (stream->start[stream->idx + 1] == ':' &&
            ((stream->start[stream->idx + 2] == '%' &&
              stream->start[stream->idx + 3] == ':') ||
             stream->start[stream->idx + 2] == '#')) {
            return (Lex){.type = LEX_HashHash, .span = from_stream(stream, 4)};
        } else if (stream->start[stream->idx + 1] == '=')
            return (Lex){.type = LEX_PercentEqual,
                         .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == '>')
            return (Lex){.type = LEX_RSquigly, .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == ':')
            return macro(stream, id_table);
        return (Lex){.type = LEX_Percent, .span = from_stream(stream, 1)};
    case '<':
        if (stream->start[stream->idx + 1] == '<') {
            if (stream->start[stream->idx + 2] == '=')
                return (Lex){.type = LEX_LeftLeftEqual,
                             .span = from_stream(stream, 3)};
            return (Lex){.type = LEX_LeftLeft, .span = from_stream(stream, 2)};
        } else if (stream->start[stream->idx + 1] == '=')
            return (Lex){.type = LEX_LeftEqual, .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == ':')
            return (Lex){.type = LEX_LBracket, .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == '%')
            return (Lex){.type = LEX_LSquigly, .span = from_stream(stream, 2)};
        return (Lex){.type = LEX_Left, .span = from_stream(stream, 1)};
    case '>':
        if (stream->start[stream->idx + 1] == '>') {
            if (stream->start[stream->idx + 2] == '=')
                return (Lex){.type = LEX_RightRightEqual,
                             .span = from_stream(stream, 3)};
            return (Lex){.type = LEX_RightRight,
                         .span = from_stream(stream, 2)};
        } else if (stream->start[stream->idx + 1] == '=')
            return (Lex){.type = LEX_RightEqual,
                         .span = from_stream(stream, 2)};
        return (Lex){.type = LEX_Right, .span = from_stream(stream, 1)};
    case '^':
        if (stream->start[stream->idx + 1] == '=') {
            return (Lex){.type = LEX_CaretEqual,
                         .span = from_stream(stream, 2)};
        }
        return (Lex){.type = LEX_Caret, .span = from_stream(stream, 1)};
    case '|':
        if (stream->start[stream->idx + 1] == '|')
            return (Lex){.type = LEX_PipePipe, .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == '=')
            return (Lex){.type = LEX_PipeEqual, .span = from_stream(stream, 2)};
        return (Lex){.type = LEX_Pipe, .span = from_stream(stream, 1)};
    case '?':
        return (Lex){.type = LEX_Question, .span = from_stream(stream, 1)};
    case ':':
        if (stream->start[stream->idx + 1] == ':')
            return (Lex){.type = LEX_ColonColon,
                         .span = from_stream(stream, 2)};
        else if (stream->start[stream->idx + 1] == '>')
            return (Lex){.type = LEX_RBracket, .span = from_stream(stream, 2)};
        return (Lex){.type = LEX_Colon, .span = from_stream(stream, 1)};
    case ';':
        return (Lex){.type = LEX_Semicolon, .span = from_stream(stream, 1)};
    case '=':
        if (stream->start[stream->idx + 1] == '=') {
            return (Lex){.type = LEX_EqualEqual,
                         .span = from_stream(stream, 2)};
        }
        return (Lex){.type = LEX_Equal, .span = from_stream(stream, 1)};
    case ',':
        return (Lex){.type = LEX_Comma, .span = from_stream(stream, 1)};
    case '#':
        return macro(stream, id_table);
    default:
        return (Lex){0};
    }
}

// TODO: There is also multibyte chars
Lex constant_char(const Stream *stream, enum lex_type char_lex) {
    size_t offset;
    switch (char_lex) {
    case LEX_ConstantChar:
        offset = 0;
        break;
    case LEX_ConstantCharU8:
        offset = 2;
        break;
    case LEX_ConstantCharU16:
    case LEX_ConstantCharU32:
    case LEX_ConstantCharWide:
        offset = 1;
        break;
    default:
        return (Lex){.type = LEX_Invalid,
                     .span = from_stream(stream, 1),
                     .invalid = IllegalChar};
    }

    if (stream->start[stream->idx + 1] == '\\') {
        char c;
        switch (stream->start[stream->idx + 2]) {
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
            if (oct_digit(stream->start[stream->idx + 2])) {
                if (oct_digit(stream->start[stream->idx + 3])) {
                    if (oct_digit(stream->start[stream->idx + 4]) &&
                        stream->start[stream->idx + 5] == '\'') {
                        return (Lex){
                            .type = char_lex,
                            .span =
                                from_stream_off(stream, -offset, 6 + offset),
                            .constant =
                                (stream->start[stream->idx + 2] - '0') * 64 +
                                (stream->start[stream->idx + 3] - '0') * 8 +
                                (stream->start[stream->idx + 4] - '0')};
                    } else if (stream->start[stream->idx + 4] == '\'') {
                        return (Lex){
                            .type = char_lex,
                            .span =
                                from_stream_off(stream, -offset, 5 + offset),
                            .constant =
                                (stream->start[stream->idx + 2] - '0') * 8 +
                                (stream->start[stream->idx + 3] - '0')};
                    }
                    return (Lex){
                        .type = LEX_Invalid,
                        .span = from_stream_off(stream, -offset, 4 + offset),
                        .invalid = IllegalEscapeChar};
                } else if (stream->start[stream->idx + 3] == '\'') {
                    return (Lex){
                        .type = char_lex,
                        .span = from_stream_off(stream, -offset, 4 + offset),
                        .constant = stream->start[stream->idx + 2] - '0'};
                }
            }
        }
        return (Lex){.type = LEX_Invalid,
                     .span = from_stream_off(stream, -offset, 2 + offset),
                     .invalid = IllegalEscapeChar};
    Single_char:
        if (stream->start[stream->idx + 3] == '\'') {
            return (Lex){.type = char_lex,
                         .span = from_stream_off(stream, -offset, 4 + offset),
                         .constant = c};
        }
        return (Lex){.type = LEX_Invalid,
                     .span = from_stream_off(stream, -offset, 3 + offset),
                     .invalid = UnfinishedChar};
    }

    if (stream->start[stream->idx + 2] == '\'') {
        return (Lex){.type = char_lex,
                     .span = from_stream_off(stream, -offset, 3 + offset),
                     .constant = stream->start[stream->idx + 1]};
    }

    return (Lex){.type = LEX_Invalid,
                 .span = from_stream_off(stream, -offset, 2 + offset),
                 .invalid = UnfinishedChar};
}

enum lex_type get_integer_suffix(const char *input, size_t *len) {
    switch (input[*len]) {
    case 'u':
    case 'U':
        switch (input[*len + 1]) {
        case 'l':
            if (input[*len + 2] == 'l') {
                *len += 3;
                return LEX_ConstantUnsignedLongLong;
            } else {
                *len += 2;
                return LEX_ConstantUnsignedLong;
            }
        case 'L':
            if (input[*len + 2] == 'L') {
                *len += 3;
                return LEX_ConstantUnsignedLongLong;
            } else {
                *len += 2;
                return LEX_ConstantUnsignedLong;
            }
        case 'w':
        case 'W':
            *len += 2;
            return LEX_ConstantUnsignedBitPrecise;
        default:
            *len += 1;
            return LEX_ConstantUnsigned;
        }
    case 'l':
        if (input[*len + 1] == 'l') {
            if (input[*len + 2] == 'u' || input[*len + 2] == 'U') {
                *len += 3;
                return LEX_ConstantUnsignedLongLong;
            }
            *len += 2;
            return LEX_ConstantLongLong;
        } else if (input[*len + 1] == 'u' || input[*len + 1] == 'U') {
            *len += 2;
            return LEX_ConstantUnsignedLong;
        }
        *len += 1;
        return LEX_ConstantLong;
    case 'L':
        if (input[*len + 1] == 'L') {
            if (input[*len + 2] == 'u' || input[*len + 2] == 'U') {
                *len += 3;
                return LEX_ConstantUnsignedLongLong;
            }
            *len += 2;
            return LEX_ConstantLongLong;
        } else if (input[*len + 1] == 'u' || input[*len + 1] == 'U') {
            *len += 2;
            return LEX_ConstantUnsignedLong;
        }
        *len += 1;
        return LEX_ConstantLong;
    case 'w':
    case 'W':
        if (input[*len + 1] == 'u' || input[*len + 1] == 'U') {
            *len += 2;
            return LEX_ConstantUnsignedBitPrecise;
        }
        *len += 1;
        return LEX_ConstantBitPrecise;
    default:
        return LEX_Constant;
    }
}

enum lex_type get_float_suffix(const char *input, size_t *len) {
    switch (input[*len]) {
    case 'f':
    case 'F':
        *len += 1;
        return LEX_ConstantFloat;
    case 'l':
    case 'L':
        *len += 1;
        return LEX_ConstantLongDouble;
    case 'd':
        switch (input[*len + 1]) {
        case 'f':
            *len += 2;
            return LEX_ConstantDecimal32;
        case 'd':
            *len += 2;
            return LEX_ConstantDecimal64;
        case 'l':
            *len += 2;
            return LEX_ConstantDecimal128;
        }
    case 'D':
        switch (input[*len + 1]) {
        case 'F':
            *len += 2;
            return LEX_ConstantDecimal32;
        case 'D':
            *len += 2;
            return LEX_ConstantDecimal64;
        case 'L':
            *len += 2;
            return LEX_ConstantDecimal128;
        }
    default:
        return LEX_ConstantDouble;
    }
}

// TODO: Faster and/or more accurate float conversion
Lex dec_float_constant(char *input, size_t *len, size_t row, size_t col,
                       uint64_t root) {
    int64_t root_pow = 0;

    while (digit(input[*len])) {
        root *= 10;
        root += input[*len] - '0';
        root_pow -= 1;

        *len += 1;
        if (input[*len] == '\'' && digit(input[*len + 1])) {
            *len += 1;
        }
    }

    int64_t exp = 0;
    if (input[*len] == 'e' || input[*len] == 'E') {
        *len += 1;
        int64_t sign = 1;

        if (input[*len] == '+') {
            *len += 1;
        } else if (input[*len] == '-') {
            *len += 1;
            sign = -1;
        }

        if (!digit(input[*len])) {
            return (Lex){.type = LEX_Invalid,
                         .span = {input, *len, row, col},
                         .invalid = IllegalFloat};
        }

        while (digit(input[*len])) {
            exp *= 10;
            exp += input[*len] - '0';

            *len += 1;
            if (input[*len] == '\'' && digit(input[*len + 1])) {
                *len += 1;
            }
        }

        exp *= sign;
    }

    enum lex_type suffixed = get_float_suffix(input, len);

    return (Lex){.type = suffixed,
                 .span = {input, *len, row, col},
                 .floating = ((long double)root) * pow(10, exp + root_pow)};
}

Lex dec_or_float_constant(char *input, size_t *len, size_t row, size_t col) {
    uint64_t constant = input[*len] - '0';
    *len += 1;

    while (digit(input[*len])) {
        constant *= 10;
        constant += input[*len] - '0';

        *len += 1;
        if (input[*len] == '\'' && digit(input[*len + 1])) {
            *len += 1;
        }
    }

    if (input[*len] == '.') {
        *len += 1;
        return dec_float_constant(input, len, row, col, constant);
    } else if (input[*len] == 'e' || input[*len] == 'E') {
        return dec_float_constant(input, len, row, col, constant);
    }

    enum lex_type suffixed = get_integer_suffix(input, len);

    return (Lex){.type = suffixed,
                 .span = {input, *len, row, col},
                 .constant = constant};
}

// TODO: This needs a proper implementation
// NOTE: Exponent is mandatory for hex but not dec
Lex hex_float_constant(char *input, size_t *len, size_t row, size_t col,
                       uint64_t root) {
    int64_t root_pow = 0;

    while (hex_digit(input[*len])) {
        root *= 16;
        root += hex_acquire(input[*len]);
        root_pow -= 1;

        *len += 1;
        if (input[*len] == '\'' && hex_digit(input[*len + 1])) {
            *len += 1;
        }
    }

    int64_t exp = 0;
    if (input[*len] == 'p' || input[*len] == 'P') {
        *len += 1;
        int64_t sign = 1;

        if (input[*len] == '+') {
            *len += 1;
        } else if (input[*len] == '-') {
            *len += 1;
            sign = -1;
        }

        if (!hex_digit(input[*len])) {
            return (Lex){.type = LEX_Invalid,
                         .span = {input, *len, row, col},
                         .invalid = IllegalFloat};
        }

        while (hex_digit(input[*len])) {
            exp *= 16;
            exp += hex_acquire(input[*len]);

            *len += 1;
            if (input[*len] == '\'' && hex_digit(input[*len + 1])) {
                *len += 1;
            }
        }

        exp *= sign;
    } else {
        return (Lex){.type = LEX_Invalid,
                     .span = {input, *len, row, col},
                     .invalid = IllegalFloatHexExponent};
    }

    enum lex_type suffixed = get_float_suffix(input, len);
    if (suffixed == LEX_ConstantDecimal32 ||
        suffixed == LEX_ConstantDecimal64 ||
        suffixed == LEX_ConstantDecimal128) {
        return (Lex){.type = LEX_Invalid,
                     .span = {input, *len, row, col},
                     .invalid = IllegalFloatHexSuffix};
    }

    return (Lex){.type = suffixed,
                 .span = {input, *len, row, col},
                 .floating =
                     ((long double)root) * ((exp + root_pow >= 0)
                                                ? 2 << (exp + root_pow * 4)
                                                : 2 >> (exp + root_pow * 4))};
}

Lex hex_or_float_constant(char *input, size_t *len, size_t row, size_t col) {
    uint64_t constant = hex_acquire(input[*len]);
    *len += 1;

    while (hex_digit(input[*len])) {
        constant *= 16;
        constant += hex_acquire(input[*len]);

        *len += 1;
        if (input[*len] == '\'' && hex_digit(input[*len + 1])) {
            *len += 1;
        }
    }
    if (input[*len] == '.') {
        *len += 1;
        return dec_float_constant(input, len, row, col, constant);
    } else if (input[*len] == 'p' || input[*len] == 'P') {
        return dec_float_constant(input, len, row, col, constant);
    }

    enum lex_type suffixed = get_integer_suffix(input, len);

    return (Lex){.type = suffixed,
                 .span = {input, *len, row, col},
                 .constant = constant};
}

Lex num_constant(const Stream *stream, char *input, size_t *len, uint64_t base,
                 int (*xdigit)(char c), uint64_t (*xacquire)(char c)) {
    uint64_t constant = xacquire(stream->start[stream->idx]);

    while (xdigit(input[*len])) {
        constant *= base;
        constant += xacquire(input[*len]);

        *len += 1;
        if (input[*len] == '\'' && xdigit(input[*len + 1])) {
            *len += 1;
        }
    }

    enum lex_type suffixed = get_integer_suffix(input, len);

    return (Lex){.type = suffixed,
                 .span = {input, *len, stream->row, stream->col},
                 .constant = constant};
}

Lex constant(Stream *stream) {
    switch (stream->start[stream->idx]) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
        size_t len = 0;
        return dec_or_float_constant(stream->start + stream->idx, &len,
                                     stream->row, stream->col);
        break;
    }
    case '.':
        if (digit(stream->start[stream->idx + 1])) {
            size_t len = 1;
            return dec_float_constant(stream->start + stream->idx, &len,
                                      stream->row, stream->col, 0);
        }
        break;
    case '0':
        if ((stream->start[stream->idx + 1] == 'x' ||
             stream->start[stream->idx + 1] == 'X')) {
            if (hex_digit(stream->start[stream->idx + 2])) {
                size_t len = 2;
                return hex_or_float_constant(stream->start + stream->idx, &len,
                                             stream->row, stream->col);
            } else if (stream->start[stream->idx + 2] == '.') {
                size_t len = 2;
                return hex_float_constant(stream->start + stream->idx, &len,
                                          stream->row, stream->col, 0);
            }
        } else if ((stream->start[stream->idx + 1] == 'b' ||
                    stream->start[stream->idx + 1] == 'B') &&
                   bin_digit(stream->start[stream->idx + 2])) {
            size_t len = 3;
            Stream local = *stream;
            local.idx += 2;
            return num_constant(&local, stream->start + stream->idx, &len, 2,
                                bin_digit, acquire);
        } else if (oct_digit(stream->start[stream->idx + 1])) {
            size_t len = 2;
            Stream local = *stream;
            local.idx += 1;
            return num_constant(&local, stream->start + stream->idx, &len, 8,
                                oct_digit, acquire);
        } else {
            size_t len = 1;
            enum lex_type suffixed = get_integer_suffix(stream->start, &len);
            return (Lex){.type = suffixed,
                         .span = from_stream(stream, len),
                         .constant = 0};
        }
        break;
    case '\'':
        return constant_char(stream, LEX_ConstantChar);
        break;
        // TODO: These need more proper handling
    case 'u':
        if (stream->start[stream->idx + 1] == '8' &&
            stream->start[stream->idx + 2] == '\'') {
            Stream local = *stream;
            local.idx += 2;
            return constant_char(&local, LEX_ConstantCharU8);
        } else if (stream->start[stream->idx + 1] == '\'') {
            Stream local = *stream;
            local.idx += 1;
            return constant_char(&local, LEX_ConstantCharU16);
        }
        break;
    case 'U':
        if (stream->start[stream->idx + 1] == '\'') {
            Stream local = *stream;
            local.idx += 1;
            return constant_char(&local, LEX_ConstantCharU32);
        }
        break;
    case 'L':
        if (stream->start[stream->idx + 1] == '\'') {
            Stream local = *stream;
            local.idx += 1;
            return constant_char(&local, LEX_ConstantCharWide);
        }
        break;
    default:
        break;
    }

    return (Lex){0};
}

Lex string_ret(const Stream *stream, size_t limit, enum lex_type str_lex,
               Vector **id_table) {
    const char *input = stream->start + stream->idx;
    size_t len = 1;
    size_t offset;
    switch (str_lex) {
    case LEX_StringU8:
        offset = 2;
        break;
    case LEX_StringU16:
    case LEX_StringU32:
    case LEX_StringWide:
        offset = 1;
        break;
    case LEX_String:
        offset = 0;
        break;
    default:
        return (Lex){.type = LEX_Invalid,
                     .span = from_stream(stream, len),
                     .invalid = IllegalString};
    }

    while (input[len] != '"') {
        if (input[len] == '\\' && input[len + 1] == '"') {
            len += 2;
        } else {
            len += 1;
        }

        if (limit <= len) {
            return (Lex){.type = LEX_Invalid,
                         .span = from_stream_off(stream, -offset, len + offset),
                         .invalid = IllegalString};
        }
    }

    len += 1;

    return (Lex){.type = str_lex,
                 .span = from_stream_off(stream, -offset, len + offset),
                 .id = search_id_table(from_stream(stream, len), id_table)};
}

Lex string(const Stream *stream, size_t limit, Vector **id_table) {
    Stream local = *stream;
    if (local.start[local.idx] == '"') {
        return string_ret(&local, limit, LEX_String, id_table);
    } else if (local.start[local.idx] == 'u') {
        if (local.start[local.idx + 1] == '8' &&
            local.start[local.idx + 2] == '"') {
            local.idx += 2;
            return string_ret(&local, limit - 2, LEX_StringU8, id_table);
        } else if (local.start[local.idx + 1] == '"') {
            local.idx += 1;
            return string_ret(&local, limit - 1, LEX_StringU16, id_table);
        }
    } else if (local.start[local.idx] == 'U' &&
               local.start[local.idx + 1] == '"') {
        local.idx += 1;
        return string_ret(&local, limit - 1, LEX_StringU32, id_table);
    } else if (local.start[local.idx] == 'L' &&
               local.start[local.idx + 1] == '"') {
        local.idx += 1;
        return string_ret(&local, limit - 1, LEX_StringWide, id_table);
    }

    return (Lex){0};
}

Lex comment(Stream *stream, size_t limit) {
    if (stream->start[stream->idx] == '/') {
        if (stream->start[stream->idx + 1] == '/') {
            const char *input = stream->start + stream->idx;
            size_t len = 2;
            while (input[len] != '\n') {
                len += 1;
                if (limit <= len) {
                    return (Lex){.type = LEX_Eof};
                }
            }
            stream->row += 1;
            stream->col = 0;
            return (Lex){.type = LEX_Comment, .span = from_stream(stream, len)};
        } else if (stream->start[stream->idx + 1] == '*') {
            const char *input = stream->start + stream->idx;
            size_t len = 2;
            while (input[len] != '*' || input[len + 1] != '/') {
                if (input[len] == '\n') {
                    stream->row += 1;
                    stream->col = 0;
                }
                len += 1;
                if (limit <= len + 1) {
                    return (Lex){.type = LEX_Eof};
                }
            }

            len += 2;

            return (Lex){.type = LEX_Comment, .span = from_stream(stream, len)};
        }
    }

    return (Lex){0};
}

Lex lex_next(Stream *stream, Ids **id_table) {
    while (stream->idx < stream->len) {
        Lex key = keyword_or_id(stream, id_table);
        if (key.type || key.invalid) {
            stream->idx += key.span.len;
            stream->col += key.span.len;
            return key;
        }

        Lex cnst = constant(stream);
        if (cnst.type || cnst.invalid) {
            stream->idx += cnst.span.len;
            stream->col += cnst.span.len;
            return cnst;
        }

        Lex str = string(stream, stream->len - stream->idx, id_table);
        if (str.type || str.invalid) {
            stream->idx += str.span.len;
            stream->col += str.span.len;
            return str;
        }

        Lex comm = comment(stream, stream->len - stream->idx);
        if (comm.type) {
            if (comm.type == LEX_Eof) {
                return comm;
            } else {
                stream->idx += comm.span.len;
                continue;
            }
        }

        Lex punct = punctuator(stream, id_table);
        if (punct.type || punct.invalid) {
            stream->idx += punct.span.len;
            stream->col += punct.span.len;
            return punct;
        }

        if (!space(stream->start[stream->idx])) {
            stream->idx += 1;
            stream->col += 1;
            return (Lex){.type = LEX_Invalid,
                         .span = from_stream_off(stream, -1, 1),
                         .invalid = IllegalToken};
        }

        if (stream->macro_line && stream->start[stream->idx] == '\\' &&
            stream->start[stream->idx + 1] == '\n') {
            stream->idx += 2;
            stream->row += 1;
            stream->col = 0;
        } else if (stream->start[stream->idx] == '\n') {
            if (stream->macro_line) {
                stream->macro_line = 0;
                return (Lex){0};
            }
            stream->idx += 1;
            stream->row += 1;
            stream->col = 0;
        } else {
            stream->idx += 1;
            stream->col += 1;
        }
    }

    return (Lex){.type = LEX_Eof};
}

Span from_stream(const Stream *stream, size_t len) {
    return (Span){.start = (char *)stream->start + stream->idx,
                  .len = len,
                  .row = stream->row,
                  .col = stream->col};
}

Span from_stream_off(const Stream *stream, ptrdiff_t off, size_t len) {
    return (Span){.start = (char *)stream->start + stream->idx + off,
                  .len = len,
                  .row = stream->row,
                  .col = stream->col};
}

Ids *create_ids(size_t capacity) { return create_vec(capacity, sizeof(Span)); }

Lexes *create_lexes(size_t capacity) {
    return create_vec(capacity, sizeof(Lex));
}

void print_ids(const Ids *ids) {
    for (uint64_t i = 0; i < ids->length; i++) {
        Span span = *(Span *)at_elem_vec((Ids *)ids, i);
        printf("<Id %zu %.*s>\n", i, (int)span.len, span.start);
    }
}

void print_lexes(const Lexes *lexes) {
    for (uint64_t i = 0; i < lexes->length; i++) {
        Lex lex = *(Lex *)at_elem_vec((Lexes *)lexes, i);
        switch (lex.type) {
        case LEX_Invalid:
            if (lex.invalid) {
                printf(":Lex Error %d: [%p, %zu, %zu, %zu]\n", lex.invalid,
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
            } else {
                printf(":Lex Ok: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
            }
            break;
        case LEX_Eof:
            printf(":End of File: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_StringU8:
            printf(":StringU8 id %zu: [%p, %zu, %zu, %zu]\n", lex.id,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_StringU16:
            printf(":StringU16 id %zu: [%p, %zu, %zu, %zu]\n", lex.id,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_StringU32:
            printf(":StringU32 id %zu: [%p, %zu, %zu, %zu]\n", lex.id,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_StringWide:
            printf(":StringWide id %zu: [%p, %zu, %zu, %zu]\n", lex.id,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_String:
            printf(":String id %zu: [%p, %zu, %zu, %zu]\n", lex.id,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Identifier:
            printf(":Identifier id %zu: [%p, %zu, %zu, %zu]\n", lex.id,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantUnsignedLongLong:
            printf(":UnsignedLongLongNumber %zu: [%p, %zu, %zu, %zu]\n",
                   lex.constant, lex.span.start, lex.span.len, lex.span.row,
                   lex.span.col);
            break;
        case LEX_ConstantUnsignedLong:
            printf(":UnsignedLongNumber %zu: [%p, %zu, %zu, %zu]\n",
                   lex.constant, lex.span.start, lex.span.len, lex.span.row,
                   lex.span.col);
            break;
        case LEX_ConstantUnsignedBitPrecise:
            printf(":UnsignedBitPreciseNumber %zu: [%p, %zu, %zu, %zu]\n",
                   lex.constant, lex.span.start, lex.span.len, lex.span.row,
                   lex.span.col);
            break;
        case LEX_ConstantUnsigned:
            printf(":UnsignedNumber %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantLongLong:
            printf(":LongLongNumber %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantLong:
            printf(":LongNumber %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantBitPrecise:
            printf(":BitPreciseNumber %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Constant:
            printf(":Number %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantFloat:
            printf(":Float %f: [%p, %zu, %zu, %zu]\n", lex.floating,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantDouble:
            printf(":Double %f: [%p, %zu, %zu, %zu]\n", lex.floating,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantLongDouble:
            printf(":LongDouble %f: [%p, %zu, %zu, %zu]\n", lex.floating,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantDecimal32:
            printf(":Dec32 %f: [%p, %zu, %zu, %zu]\n", lex.floating,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantDecimal64:
            printf(":Dec64 %f: [%p, %zu, %zu, %zu]\n", lex.floating,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantDecimal128:
            printf(":Dec128 %f: [%p, %zu, %zu, %zu]\n", lex.floating,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantCharU8:
            printf(":CharU8 %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantCharU16:
            printf(":CharU16 %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantCharU32:
            printf(":CharU32 %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantCharWide:
            printf(":CharWide %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ConstantChar:
            printf(":Char %zu: [%p, %zu, %zu, %zu]\n", lex.constant,
                   lex.span.start, lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Keyword:
            switch (lex.key) {
            case KEY_alignas:
                printf(":Keyword alignas: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_alignof:
                printf(":Keyword alignof: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_auto:
                printf(":Keyword auto: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_bool:
                printf(":Keyword bool: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_break:
                printf(":Keyword break: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_case:
                printf(":Keyword case: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_char:
                printf(":Keyword char: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_const:
                printf(":Keyword const: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_constexpr:
                printf(":Keyword constexpr: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_continue:
                printf(":Keyword continue: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_default:
                printf(":Keyword default: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_do:
                printf(":Keyword do: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_double:
                printf(":Keyword double: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_else:
                printf(":Keyword else: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_enum:
                printf(":Keyword enum: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_extern:
                printf(":Keyword extern: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_false:
                printf(":Keyword false: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_float:
                printf(":Keyword float: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_for:
                printf(":Keyword for: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_goto:
                printf(":Keyword goto: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_if:
                printf(":Keyword if: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_inline:
                printf(":Keyword inline: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_int:
                printf(":Keyword int: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_long:
                printf(":Keyword long: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_nullptr:
                printf(":Keyword nullptr: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_register:
                printf(":Keyword register: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_restrict:
                printf(":Keyword restrict: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_return:
                printf(":Keyword return: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_short:
                printf(":Keyword short: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_signed:
                printf(":Keyword signed: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_sizeof:
                printf(":Keyword sizeof: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_static:
                printf(":Keyword static: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_static_assert:
                printf(":Keyword static: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_struct:
                printf(":Keyword struct: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_switch:
                printf(":Keyword switch: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_thread_local:
                printf(":Keyword thread: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_true:
                printf(":Keyword true: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_typedef:
                printf(":Keyword typedef: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_typeof:
                printf(":Keyword typeof: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_typeof_unqual:
                printf(":Keyword typeof: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_union:
                printf(":Keyword union: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_unsigned:
                printf(":Keyword unsigned: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_void:
                printf(":Keyword void: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY_volatile:
                printf(":Keyword volatile: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_while:
                printf(":Keyword while: [%p, %zu, %zu, %zu]\n", lex.span.start,
                       lex.span.len, lex.span.row, lex.span.col);
                break;
            case KEY__Atomic:
                printf(":Keyword _Atomic: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY__BitInt:
                printf(":Keyword _BitInt: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY__Complex:
                printf(":Keyword _Complex: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY__Decimal128:
                printf(":Keyword _Decimal128: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY__Decimal32:
                printf(":Keyword _Decimal32: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY__Decimal64:
                printf(":Keyword _Decimal64: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY__Generic:
                printf(":Keyword _Generic: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY__Imaginary:
                printf(":Keyword _Imaginary: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY__Noreturn:
                printf(":Keyword _Noreturn: [%p, %zu, %zu, %zu]\n",
                       lex.span.start, lex.span.len, lex.span.row,
                       lex.span.col);
                break;
            case KEY_COUNT:
                break;
            }
            break;
        case LEX_MacroToken:
            printf(":MacroToken: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_LBracket:
            printf(":LBracket: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_RBracket:
            printf(":RBracket: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_LParen:
            printf(":LParen: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_RParen:
            printf(":RParen: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_LSquigly:
            printf(":LSquigly: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_RSquigly:
            printf(":RSquigly: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Dot:
            printf(":Dot: [%p, %zu, %zu, %zu]\n", lex.span.start, lex.span.len,
                   lex.span.row, lex.span.col);
            break;
        case LEX_Arrow:
            printf(":Arrow: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_PlusPlus:
            printf(":PlusPlus: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_MinusMinus:
            printf(":MinusMinus: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Et:
            printf(":Et: [%p, %zu, %zu, %zu]\n", lex.span.start, lex.span.len,
                   lex.span.row, lex.span.col);
            break;
        case LEX_Star:
            printf(":Star: [%p, %zu, %zu, %zu]\n", lex.span.start, lex.span.len,
                   lex.span.row, lex.span.col);
            break;
        case LEX_Plus:
            printf(":Plus: [%p, %zu, %zu, %zu]\n", lex.span.start, lex.span.len,
                   lex.span.row, lex.span.col);
            break;
        case LEX_Minus:
            printf(":Minus: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Tilde:
            printf(":Tilde: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Exclamation:
            printf(":Exclamation: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Slash:
            printf(":Slash: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Percent:
            printf(":Percent: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_LeftLeft:
            printf(":LeftLeft: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_RightRight:
            printf(":RightRight: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Left:
            printf(":Left: [%p, %zu, %zu, %zu]\n", lex.span.start, lex.span.len,
                   lex.span.row, lex.span.col);
            break;
        case LEX_Right:
            printf(":Right: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_LeftEqual:
            printf(":LeftEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_RightEqual:
            printf(":RightEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_EqualEqual:
            printf(":EqualEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ExclamationEqual:
            printf(":ExclamationEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Caret:
            printf(":Caret: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Pipe:
            printf(":Pipe: [%p, %zu, %zu, %zu]\n", lex.span.start, lex.span.len,
                   lex.span.row, lex.span.col);
            break;
        case LEX_EtEt:
            printf(":EtEt: [%p, %zu, %zu, %zu]\n", lex.span.start, lex.span.len,
                   lex.span.row, lex.span.col);
            break;
        case LEX_PipePipe:
            printf(":PipePipe: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Question:
            printf(":Question: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Colon:
            printf(":Colon: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_ColonColon:
            printf(":ColonColon: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Semicolon:
            printf(":Semicolon: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_DotDotDot:
            printf(":DotDotDot: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Equal:
            printf(":Equal: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_StarEqual:
            printf(":StarEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_SlashEqual:
            printf(":SlashEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_PercentEqual:
            printf(":PercentEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_PlusEqual:
            printf(":PlusEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_MinusEqual:
            printf(":MinusEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_LeftLeftEqual:
            printf(":LeftLeftEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_RightRightEqual:
            printf(":RightRightEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_EtEqual:
            printf(":EtEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_CaretEqual:
            printf(":CaretEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_PipeEqual:
            printf(":PipeEqual: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Comma:
            printf(":Comma: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Hash:
            printf(":Hash: [%p, %zu, %zu, %zu]\n", lex.span.start, lex.span.len,
                   lex.span.row, lex.span.col);
            break;
        case LEX_HashHash:
            printf(":HashHash: [%p, %zu, %zu, %zu]\n", lex.span.start,
                   lex.span.len, lex.span.row, lex.span.col);
            break;
        case LEX_Comment:
            break;
        }
    }
}
