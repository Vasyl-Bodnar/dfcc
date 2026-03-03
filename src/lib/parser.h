/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PARSER_H_
#define PARSER_H_

#include "lexer.h"
#include "pp.h"
#include "vec.h"

typedef Vector Tree;     // Asts
typedef Vector IdxStack; // Stack of Idx to Ctx

// Note that while some operations like AST_AndExpr operate on flat lists
// Operations like AST_Equal are binary only
enum ast_type {
    AST_Invalid = 0,
    AST_Eof,
    AST_Identifier,
    AST_Constant,
    AST_String,
    AST_Expr,
    AST_AssignExpr,
    AST_CondExpr,
    AST_LogOrExpr,
    AST_LogAndExpr,
    AST_InclOrExpr,
    AST_ExclOrExpr,
    AST_AndExpr,
    AST_EqualExpr,
    AST_NotEqualExpr,
    AST_LessExpr,
    AST_GreaterExpr,
    AST_LessEqExpr,
    AST_GreaterEqExpr,
};

enum num_type {
    UnsignedLongLong = 0,
    UnsignedLong,
    UnsignedBitPrecise,
    Unsigned,
    LongLong,
    Long,
    BitPrecise,
    Int,
    Float,
    Double,
    LongDouble,
    Decimal32,
    Decimal64,
    Decimal128,
    Char,
    CharU8,
    CharU16,
    CharU32,
    CharWide,
};

enum str_type { ASCII = 0, U8, U16, U32, Wide };

enum inv_type {
    BadPrimaryExpression,
    BadPrimaryExpressionRParen,
    BadExpression,
    BadConditionalExpression,
};

enum assign_op {
    Assign = 0,
    AssignMul,
    AssignDiv,
    AssignMod,
    AssignAdd,
    AssignSub,
    AssignLShift,
    AssignRShift,
    AssignAnd,
    AssignXor,
    AssignOr
};

// Consider using a flat array and indexes for Ast
// Better performance, easier to free, etc.
typedef struct Ast {
    enum ast_type type;
    Span span;
    union {
        Tree *expr; // General option, contents depend on type
        size_t id;
        enum inv_type invalid;
        struct {
            enum str_type type;
            size_t id;
        } string;
        struct {
            enum num_type type;
            union {
                uint64_t c;
                double d;
            };
        } constant;
        struct {
            Tree *assigns; // Unary and sub AssignExpr
            enum assign_op op;
        } assign;
    };
} Ast;

// Ctx and idxes are for backtracking purposes
typedef struct Parser {
    Lexes *ctx;
    size_t idx;
    IdxStack *idx_stack;
    Preprocessor pp;
} Parser;

Ast parse(Parser *pp);

Parser *create_parser(String *file_path);
void print_parser(Parser *parser);
void delete_parser(Parser *parser);

Tree *create_tree(size_t capacity);
void print_tree(Tree *tree, int depth);
void delete_tree(Tree *tree);

#endif // PARSER_H_
