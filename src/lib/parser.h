#ifndef PARSER_H_
#define PARSER_H_

#include "lexer.h"
#include "pp.h"
#include "vec.h"

typedef Vector Tree;     // Asts
typedef Vector IdxStack; // Stack of Idx to Ctx

enum ast_type {
    AST_Invalid,
    AST_Eof,
    AST_Identifier,
    AST_Constant,
    AST_String,
    AST_Expr,
    AST_AssignExpr,
};

enum num_type {
    UnsignedBitPrecise,
    UnsignedLongLong,
    UnsignedLong,
    Unsigned,
    BitPrecise,
    LongLong,
    Long,
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

enum str_type { ASCII, U8, U16, U32, Wide };

enum inv_type {
    BadPrimaryExpression,
    BadExpression,
};

enum assign_op {
    Assign,
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

typedef struct Ast {
    enum ast_type type;
    Span span;
    union {
        Tree *expr;
        size_t id;
        struct {
            enum inv_type type;
        } invalid;
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
void print_tree(Tree *tree);
void delete_tree(Tree *tree);

#endif // PARSER_H_
