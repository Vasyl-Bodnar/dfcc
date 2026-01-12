#ifndef LEXER_H_
#define LEXER_H_

#include "vec.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum lex_type {
    Invalid = 0,
    Eof,
    Keyword,
    Identifier,
    String,
    AdjacentString,
    MacroToken,
    Constant,
    LBracket, // Also <:
    RBracket, // Also :>
    LParen,
    RParen,
    LSquigly, // Also <%
    RSquigly, // Also %>
    Dot,
    Arrow,
    PlusPlus,
    MinusMinus,
    Et,
    Star,
    Plus,
    Minus,
    Tilde,
    Exclamation,
    Slash,
    Percent,
    LeftLeft,   // <<
    RightRight, // >>
    Left,       // <
    Right,      // >
    LeftEqual,
    RightEqual,
    EqualEqual,
    ExclamationEqual,
    Caret,
    Pipe,
    EtEt,
    PipePipe,
    Question,
    Colon,
    ColonColon,
    Semicolon,
    DotDotDot,
    Equal,
    StarEqual,
    SlashEqual,
    PercentEqual,
    PlusEqual,
    MinusEqual,
    LeftLeftEqual,
    RightRightEqual,
    EtEqual,
    CaretEqual,
    PipeEqual,
    Comma,
    Hash,     // Also %:
    HashHash, // Also %:%:
};

// TODO: Expand for better errors
enum invalid_type {
    Ok = 0,
    IllegalChar,
};

enum lex_keyword {
    KEY_alignas = 0,
    KEY_alignof,
    KEY_auto,
    KEY_bool,
    KEY_break,
    KEY_case,
    KEY_char,
    KEY_const,
    KEY_constexpr,
    KEY_continue,
    KEY_default,
    KEY_do,
    KEY_double,
    KEY_else,
    KEY_enum,
    KEY_extern,
    KEY_false,
    KEY_float,
    KEY_for,
    KEY_goto,
    KEY_if,
    KEY_inline,
    KEY_int,
    KEY_long,
    KEY_nullptr,
    KEY_register,
    KEY_restrict,
    KEY_return,
    KEY_short,
    KEY_signed,
    KEY_sizeof,
    KEY_static,
    KEY_static_assert,
    KEY_struct,
    KEY_switch,
    KEY_thread_local,
    KEY_true,
    KEY_typedef,
    KEY_typeof,
    KEY_typeof_unqual,
    KEY_union,
    KEY_unsigned,
    KEY_void,
    KEY_volatile,
    KEY_while,
    KEY__Atomic,
    KEY__BitInt,
    KEY__Complex,
    KEY__Decimal128,
    KEY__Decimal32,
    KEY__Decimal64,
    KEY__Generic,
    KEY__Imaginary,
    KEY__Noreturn,
    KEY_COUNT,
};

enum macro_type {
    Undefined = 0,
    Include,
    Define,
    DefineFun,
    If,
    IfDefined,
    IfNotDefined,
    Else,
    ElseIf,
    ElseIfDefined,
    ElseIfNotDefined,
    Line,
    LineFile,
    Embed,
    Error,
    Warning,
    Pragma,
};

typedef struct Span {
    size_t start;
    size_t len;
} Span;

typedef struct Lex {
    enum lex_type type;
    Span span;
    union {
        enum lex_keyword key;
        enum macro_type macro;
        enum invalid_type invalid;
        size_t id;
        uint64_t constant;
    };
} Lex;

// TODO: All this macro stuff should belong to preprocessor not lexer

enum include_type {
    Hheader,
    Qheader,
};

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

typedef Vector Ids;
typedef Vector Lexes;

typedef struct MacroLex {
    enum macro_lex_type type;
    union {
        size_t id;
        uint64_t constant;
    };
} MacroLex;

typedef Vector MacroLexes;

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

// Last character must be a newline
Lex lex_next(const char *input, size_t len, size_t *idx, Ids **ids);

void print_lexes(const Lexes *lexes);

void print_ids(const char *input, const Ids *ids);

#endif // LEXER_H_
