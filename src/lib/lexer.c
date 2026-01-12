#include "lexer.h"

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

// MINOR: Possibly handle XID_Start and XID_Continue
Lex keyword_or_id(const char *input, size_t idx, size_t limit,
                  Vector **id_table) {
    if (nondigit(input[idx])) {
        Span span = {idx, 1};

        while (nondigit_or_digit(input[span.start + span.len])) {
            span.len += 1;
        }

        Lex keyword = check_keyword(input, span);
        if (keyword.type) {
            return keyword;
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
            // Adds the id to id_table
            Lex id = keyword_or_id(input, idx + 1, limit - 1, id_table);
            if (id.type) {
                if (id.type != Keyword) {
                    if (!memcmp("define", input + id.span.start, id.span.len)) {
                    } else if (!memcmp("include", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("if", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("ifdef", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("ifndef", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("else", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("elif", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("elif", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("elifdef", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("elifndef", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("endif", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("line", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("embed", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("error", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("warning", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("pragma", input + id.span.start,
                                       id.span.len)) {
                    } else if (!memcmp("undef", input + id.span.start,
                                       id.span.len)) {
                    }
                }
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
        if (limit > 2 && input[idx + 1] == '.' && input[idx + 2] == '.') {
            return (Lex){.type = DotDotDot, .span = {.start = idx, .len = 3}};
        }
        return (Lex){.type = Dot, .span = {.start = idx, .len = 1}};
    case '&':
        if (limit > 1) {
            if (input[idx + 1] == '&')
                return (Lex){.type = EtEt, .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '=')
                return (Lex){.type = EtEqual, .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Et, .span = {.start = idx, .len = 1}};
    case '*':
        if (limit > 1 && input[idx + 1] == '=') {
            return (Lex){.type = StarEqual, .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Star, .span = {.start = idx, .len = 1}};
    case '+':
        if (limit > 1) {
            if (input[idx + 1] == '+')
                return (Lex){.type = PlusPlus,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '=')
                return (Lex){.type = PlusEqual,
                             .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Plus, .span = {.start = idx, .len = 1}};
    case '-':
        if (limit > 1) {
            if (input[idx + 1] == '>')
                return (Lex){.type = Arrow, .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '-')
                return (Lex){.type = MinusMinus,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '=')
                return (Lex){.type = MinusEqual,
                             .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Minus, .span = {.start = idx, .len = 1}};
    case '~':
        return (Lex){.type = Tilde, .span = {.start = idx, .len = 1}};
    case '!':
        if (limit > 1 && input[idx + 1] == '=') {
            return (Lex){.type = ExclamationEqual,
                         .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Exclamation, .span = {.start = idx, .len = 1}};
    case '/':
        if (limit > 1 && input[idx + 1] == '=') {
            return (Lex){.type = SlashEqual, .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Slash, .span = {.start = idx, .len = 1}};
    case '%':
        if (limit > 3 && input[idx + 1] == ':' && input[idx + 2] == '%' &&
            input[idx + 3] == ':') {
            return macro(input, idx, limit, id_table);
        }
        if (limit > 1) {
            if (input[idx + 1] == '=')
                return (Lex){.type = PercentEqual,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '>')
                return (Lex){.type = RSquigly,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == ':')
                return macro(input, idx, limit, id_table);
        }
        return (Lex){.type = Percent, .span = {.start = idx, .len = 1}};
    case '<':
        if (limit > 2 && input[idx + 1] == '<') {
            if (input[idx + 2] == '=')
                return (Lex){.type = LeftLeftEqual,
                             .span = {.start = idx, .len = 3}};
            return (Lex){.type = LeftLeft, .span = {.start = idx, .len = 2}};
        } else if (limit > 1) {
            if (input[idx + 1] == '<')
                return (Lex){.type = LeftLeft,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '=')
                return (Lex){.type = LeftEqual,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == ':')
                return (Lex){.type = LBracket,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '%')
                return (Lex){.type = LSquigly,
                             .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Left, .span = {.start = idx, .len = 1}};
    case '>':
        if (limit > 2 && input[idx + 1] == '>') {
            if (input[idx + 2] == '=')
                return (Lex){.type = RightRightEqual,
                             .span = {.start = idx, .len = 3}};
            return (Lex){.type = RightRight, .span = {.start = idx, .len = 2}};
        } else if (limit > 1) {
            if (input[idx + 1] == '>')
                return (Lex){.type = RightRight,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '=')
                return (Lex){.type = RightEqual,
                             .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Right, .span = {.start = idx, .len = 1}};
    case '^':
        if (limit > 1 && input[idx + 1] == '=') {
            return (Lex){.type = CaretEqual, .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Caret, .span = {.start = idx, .len = 1}};
    case '|':
        if (limit > 1) {
            if (input[idx + 1] == '|')
                return (Lex){.type = PipePipe,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '=')
                return (Lex){.type = PipeEqual,
                             .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Pipe, .span = {.start = idx, .len = 1}};
    case '?':
        return (Lex){.type = Question, .span = {.start = idx, .len = 1}};
    case ':':
        if (limit > 1) {
            if (input[idx + 1] == ':')
                return (Lex){.type = ColonColon,
                             .span = {.start = idx, .len = 2}};
            else if (input[idx + 1] == '>')
                return (Lex){.type = RBracket,
                             .span = {.start = idx, .len = 2}};
        }
        return (Lex){.type = Colon, .span = {.start = idx, .len = 1}};
    case ';':
        return (Lex){.type = Semicolon, .span = {.start = idx, .len = 1}};
    case '=':
        if (limit > 1 && input[idx + 1] == '=') {
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

Lex num_constant(const char *input, size_t idx, size_t limit, Span span,
                 uint64_t base, int (*xdigit)(char c),
                 uint64_t (*xacquire)(char c)) {
    uint64_t constant = xacquire(input[idx]);

    while (xdigit(input[span.start + span.len])) {
        constant *= base;
        constant += xacquire(input[span.start + span.len]);

        span.len += 1;
        if (input[span.start + span.len] == '\'' && span.len + 1 < limit &&
            xdigit(input[span.start + span.len + 1])) {
            span.len += 1;
        }
    }

    return (Lex){.type = Constant, .span = span, .constant = constant};
}

// TODO: Chars, floats, suffixes
Lex constant(const char *input, size_t idx, size_t limit) {
    if (nonzero(input[idx])) {
        return num_constant(input, idx, limit, (Span){idx, 1}, 10, digit,
                            acquire);
    } else if (input[idx] == '0') {
        if (limit > 2) {
            if ((input[idx + 1] == 'x' || input[idx + 1] == 'X') &&
                hex_digit(input[idx + 2])) {
                return num_constant(input, idx + 2, limit, (Span){idx, 3}, 16,
                                    hex_digit, hex_acquire);
            } else if ((input[idx + 1] == 'b' || input[idx + 1] == 'B') &&
                       bin_digit(input[idx + 2])) {
                return num_constant(input, idx + 2, limit, (Span){idx, 3}, 2,
                                    bin_digit, acquire);
            }
        }
        if (limit > 1 && oct_digit(input[idx + 1])) {
            return num_constant(input, idx + 1, limit, (Span){idx, 2}, 8,
                                oct_digit, acquire);
        }
        return (Lex){.type = Constant, .span = {idx, 1}, .constant = 0};
    }
    return (Lex){0};
}

// TODO: Handle u8, u, U, L, also for chars
Lex string(const char *input, size_t idx, size_t limit, Vector **id_table) {
    if (input[idx] == '"') {
        Span span = {idx, 1};

        while (input[span.start + span.len] != '"') {
            span.len += 1;
        }

        span.len += 1;

        return (Lex){.type = String,
                     .span = span,
                     .id = search_id_table(input, span, id_table)};
    }
    return (Lex){0};
}

Lex lex_next(const char *input, size_t len, size_t *idx, Ids **id_table) {
    Lex lexed;
    while (*idx < len) {
        lexed = keyword_or_id(input, *idx, len - *idx, id_table);
        if (lexed.type || lexed.invalid) {
            *idx += lexed.span.len;
            return lexed;
        }

        lexed = constant(input, *idx, len - *idx);
        if (lexed.type || lexed.invalid) {
            *idx += lexed.span.len;
            return lexed;
        }

        lexed = string(input, *idx, len - *idx, id_table);
        if (lexed.type || lexed.invalid) {
            *idx += lexed.span.len;
            return lexed;
        }

        lexed = punctuator(input, *idx, len - *idx, id_table);
        if (lexed.type || lexed.invalid) {
            *idx += lexed.span.len;
            return lexed;
        }

        if (!space(input[*idx])) {
            return (Lex){
                .type = Invalid, .span = {*idx, 1}, .invalid = IllegalChar};
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
            puts(":Unhandled or Broken Lex:\n");
            break;
        case Eof:
            puts(":End of File:\n");
            break;
        case String:
            printf(":String id %zu:\n", lex.id);
            break;
        case AdjacentString:
            printf(":AdjacentString %zu:\n", lex.id);
            break;
        case Identifier:
            printf(":Identifier id %zu:\n", lex.id);
            break;
        case Constant:
            printf(":Number %zu:\n", lex.constant);
            break;
        case Keyword:
            switch (lex.key) {
            case KEY_alignas:
                puts(":Keyword alignas:");
                break;
            case KEY_alignof:
                puts(":Keyword alignof:");
                break;
            case KEY_auto:
                puts(":Keyword auto:");
                break;
            case KEY_bool:
                puts(":Keyword bool:");
                break;
            case KEY_break:
                puts(":Keyword break:");
                break;
            case KEY_case:
                puts(":Keyword case:");
                break;
            case KEY_char:
                puts(":Keyword char:");
                break;
            case KEY_const:
                puts(":Keyword const:");
                break;
            case KEY_constexpr:
                puts(":Keyword constexpr:");
                break;
            case KEY_continue:
                puts(":Keyword continue:");
                break;
            case KEY_default:
                puts(":Keyword default:");
                break;
            case KEY_do:
                puts(":Keyword do:");
                break;
            case KEY_double:
                puts(":Keyword double:");
                break;
            case KEY_else:
                puts(":Keyword else:");
                break;
            case KEY_enum:
                puts(":Keyword enum:");
                break;
            case KEY_extern:
                puts(":Keyword extern:");
                break;
            case KEY_false:
                puts(":Keyword false:");
                break;
            case KEY_float:
                puts(":Keyword float:");
                break;
            case KEY_for:
                puts(":Keyword for:");
                break;
            case KEY_goto:
                puts(":Keyword goto:");
                break;
            case KEY_if:
                puts(":Keyword if:");
                break;
            case KEY_inline:
                puts(":Keyword inline:");
                break;
            case KEY_int:
                puts(":Keyword int:");
                break;
            case KEY_long:
                puts(":Keyword long:");
                break;
            case KEY_nullptr:
                puts(":Keyword nullptr:");
                break;
            case KEY_register:
                puts(":Keyword register:");
                break;
            case KEY_restrict:
                puts(":Keyword restrict:");
                break;
            case KEY_return:
                puts(":Keyword return:");
                break;
            case KEY_short:
                puts(":Keyword short:");
                break;
            case KEY_signed:
                puts(":Keyword signed:");
                break;
            case KEY_sizeof:
                puts(":Keyword sizeof:");
                break;
            case KEY_static:
                puts(":Keyword static:");
                break;
            case KEY_static_assert:
                puts(":Keyword static:");
                break;
            case KEY_struct:
                puts(":Keyword struct:");
                break;
            case KEY_switch:
                puts(":Keyword switch:");
                break;
            case KEY_thread_local:
                puts(":Keyword thread:");
                break;
            case KEY_true:
                puts(":Keyword true:");
                break;
            case KEY_typedef:
                puts(":Keyword typedef:");
                break;
            case KEY_typeof:
                puts(":Keyword typeof:");
                break;
            case KEY_typeof_unqual:
                puts(":Keyword typeof:");
                break;
            case KEY_union:
                puts(":Keyword union:");
                break;
            case KEY_unsigned:
                puts(":Keyword unsigned:");
                break;
            case KEY_void:
                puts(":Keyword void:");
                break;
            case KEY_volatile:
                puts(":Keyword volatile:");
                break;
            case KEY_while:
                puts(":Keyword while:");
                break;
            case KEY__Atomic:
                puts(":Keyword _Atomic:");
                break;
            case KEY__BitInt:
                puts(":Keyword _BitInt:");
                break;
            case KEY__Complex:
                puts(":Keyword _Complex:");
                break;
            case KEY__Decimal128:
                puts(":Keyword _Decimal128:");
                break;
            case KEY__Decimal32:
                puts(":Keyword _Decimal32:");
                break;
            case KEY__Decimal64:
                puts(":Keyword _Decimal64:");
                break;
            case KEY__Generic:
                puts(":Keyword _Generic:");
                break;
            case KEY__Imaginary:
                puts(":Keyword _Imaginary:");
                break;
            case KEY__Noreturn:
                puts(":Keyword _Noreturn:");
                break;
            case KEY_COUNT:
                break;
            }
            break;
        case MacroToken:
            puts(":MacroToken:");
            break;
        case LBracket:
            puts(":LBracket:");
            break;
        case RBracket:
            puts(":RBracket:");
            break;
        case LParen:
            puts(":LParen:");
            break;
        case RParen:
            puts(":RParen:");
            break;
        case LSquigly:
            puts(":LSquigly:");
            break;
        case RSquigly:
            puts(":RSquigly:");
            break;
        case Dot:
            puts(":Dot:");
            break;
        case Arrow:
            puts(":Arrow:");
            break;
        case PlusPlus:
            puts(":PlusPlus:");
            break;
        case MinusMinus:
            puts(":MinusMinus:");
            break;
        case Et:
            puts(":Et:");
            break;
        case Star:
            puts(":Star:");
            break;
        case Plus:
            puts(":Plus:");
            break;
        case Minus:
            puts(":Minus:");
            break;
        case Tilde:
            puts(":Tilde:");
            break;
        case Exclamation:
            puts(":Exclamation:");
            break;
        case Slash:
            puts(":Slash:");
            break;
        case Percent:
            puts(":Percent:");
            break;
        case LeftLeft:
            puts(":LeftLeft:");
            break;
        case RightRight:
            puts(":RightRight:");
            break;
        case Left:
            puts(":Left:");
            break;
        case Right:
            puts(":Right:");
            break;
        case LeftEqual:
            puts(":LeftEqual:");
            break;
        case RightEqual:
            puts(":RightEqual:");
            break;
        case EqualEqual:
            puts(":EqualEqual:");
            break;
        case ExclamationEqual:
            puts(":ExclamationEqual:");
            break;
        case Caret:
            puts(":Caret:");
            break;
        case Pipe:
            puts(":Pipe:");
            break;
        case EtEt:
            puts(":EtEt:");
            break;
        case PipePipe:
            puts(":PipePipe:");
            break;
        case Question:
            puts(":Question:");
            break;
        case Colon:
            puts(":Colon:");
            break;
        case ColonColon:
            puts(":ColonColon:");
            break;
        case Semicolon:
            puts(":Semicolon:");
            break;
        case DotDotDot:
            puts(":DotDotDot:");
            break;
        case Equal:
            puts(":Equal:");
            break;
        case StarEqual:
            puts(":StarEqual:");
            break;
        case SlashEqual:
            puts(":SlashEqual:");
            break;
        case PercentEqual:
            puts(":PercentEqual:");
            break;
        case PlusEqual:
            puts(":PlusEqual:");
            break;
        case MinusEqual:
            puts(":MinusEqual:");
            break;
        case LeftLeftEqual:
            puts(":LeftLeftEqual:");
            break;
        case RightRightEqual:
            puts(":RightRightEqual:");
            break;
        case EtEqual:
            puts(":EtEqual:");
            break;
        case CaretEqual:
            puts(":CaretEqual:");
            break;
        case PipeEqual:
            puts(":PipeEqual:");
            break;
        case Comma:
            puts(":Comma:");
            break;
        case Hash:
            puts(":Hash:");
            break;
        case HashHash:
            puts(":HashHash:");
            break;
        }
    }
}
