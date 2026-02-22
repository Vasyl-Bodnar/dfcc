/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef LEXER_H_
#define LEXER_H_

#include "vec.h"
#include <stddef.h>
#include <stdint.h>

enum lex_type {
    LEX_Invalid = 0,
    LEX_Eof,
    LEX_Comment,
    LEX_Keyword,
    LEX_Identifier,
    LEX_String,
    LEX_StringU8,
    LEX_StringU16,
    LEX_StringU32,
    LEX_StringWide,
    LEX_MacroToken,
    LEX_ConstantUnsignedLongLong,
    LEX_ConstantUnsignedLong,
    LEX_ConstantUnsignedBitPrecise,
    LEX_ConstantUnsigned,
    LEX_ConstantLongLong,
    LEX_ConstantLong,
    LEX_ConstantBitPrecise,
    LEX_Constant,
    LEX_ConstantFloat,
    LEX_ConstantDouble,
    LEX_ConstantLongDouble,
    LEX_ConstantDecimal32,
    LEX_ConstantDecimal64,
    LEX_ConstantDecimal128,
    LEX_ConstantChar,
    LEX_ConstantCharU8,
    LEX_ConstantCharU16,
    LEX_ConstantCharU32,
    LEX_ConstantCharWide,
    LEX_LBracket, // Also <:
    LEX_RBracket, // Also :>
    LEX_LParen,
    LEX_RParen,
    LEX_LSquigly, // Also <%
    LEX_RSquigly, // Also %>
    LEX_Dot,
    LEX_Arrow,
    LEX_PlusPlus,
    LEX_MinusMinus,
    LEX_Et,
    LEX_Star,
    LEX_Plus,
    LEX_Minus,
    LEX_Tilde,
    LEX_Exclamation,
    LEX_Slash,
    LEX_Percent,
    LEX_LeftLeft,   // <<
    LEX_RightRight, // >>
    LEX_Left,       // <
    LEX_Right,      // >
    LEX_LeftEqual,
    LEX_RightEqual,
    LEX_EqualEqual,
    LEX_ExclamationEqual,
    LEX_Caret,
    LEX_Pipe,
    LEX_EtEt,
    LEX_PipePipe,
    LEX_Question,
    LEX_Colon,
    LEX_ColonColon,
    LEX_Semicolon,
    LEX_DotDotDot,
    LEX_Equal,
    LEX_StarEqual,
    LEX_SlashEqual,
    LEX_PercentEqual,
    LEX_PlusEqual,
    LEX_MinusEqual,
    LEX_LeftLeftEqual,
    LEX_RightRightEqual,
    LEX_EtEqual,
    LEX_CaretEqual,
    LEX_PipeEqual,
    LEX_Comma,
    LEX_Hash,     // Also %:
    LEX_HashHash, // Also %:%:
};

// TODO: Expand for better errors
enum invalid_type {
    Ok = 0,
    // Lex
    UnfinishedChar,
    IllegalToken,
    IllegalFloat,
    IllegalFloatHexSuffix,
    IllegalFloatHexExponent,
    IllegalChar,
    IllegalEscapeChar,
    IllegalString,
    // Preprocessor
    ExpectedImpossible,       // This should not happen
    ExpectedValidMacro,       // We got something weird
    ExpectedFileNotMacro,     // Can't #define macros inside #define macro
    ExpectedStringErrorMacro, // #error has a bad string
    ExpectedStringWarnMacro,  // #warning has a bad string
    ExpectedValidIncludeFile,
    ExpectedValidIncludeMacro,
    ExpectedIncludeHeader,
    ExpectedIdMacroDefine,
    ExpectedIdMacroUndefine,
    ExpectedGoodArgsMacroDefine,
    ExpectedLessArgsMacro,
    ExpectedMoreArgsMacro,
    ExpectedIdIfDef,
    ExpectedIfEndIf,
    ExpectedIfElse,
    ExpectedIfElseDef,
    ExpectedIfElseNotDef,
    // Dark times ahead
    FailedRealloc,
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
    InvalidMacro = 0,
    Include,
    Define,
    Undefine,
    If,
    IfDefined,
    IfNotDefined,
    Else,
    ElseIf,
    ElseIfDefined,
    ElseIfNotDefined,
    EndIf,
    Line,
    Embed,
    Error,
    Warning,
    Pragma,
};

typedef struct Span {
    char *start;
    size_t len;
    size_t row;
    size_t col;
} Span;

// Span, but with an idx and macro detection
// It is legal to change row and col as well
typedef struct Stream {
    char *start;
    size_t len;
    size_t row;
    size_t col;
    size_t idx;
    // see enum macro_type, where InvalidMacro is eqv to no macro
    int macro_line;
} Stream;

typedef struct Lex {
    enum lex_type type;
    Span span;
    union {
        enum lex_keyword key;
        enum macro_type macro;
        enum invalid_type invalid;
        size_t id;
        uint64_t constant;
        double floating;
    };
} Lex;

typedef Vector Ids;
typedef Vector Lexes;

// Last character must be a newline
Lex lex_next(Stream *stream, Ids **id_table);

Span from_stream(const Stream *stream, size_t len);
Span from_stream_off(const Stream *stream, ptrdiff_t off, size_t len);

Ids *create_ids(size_t capacity);

Lexes *create_lexes(size_t capacity);

void print_lexes(const Lexes *lexes);

void print_ids(const Ids *id_table);

#endif // LEXER_H_
