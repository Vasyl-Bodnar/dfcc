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
    AST_EofInvalid, // Bad Eof e.g. [
    AST_Eof,
    // Special
    AST_LexList,       // general grouping e.g. a / b / c
    AST_Attribute,     // atr / atr() / atr(a, b) / xxx::atr / etc.
    AST_AttributeList, // [[atr0]][[atr1, atr2]] / [[atr0, atr1, atr2]] / etc.
    AST_Attributed,    // [[]] stat
    AST_IdLabel,       // L1: / Error:
    AST_CaseLabel,     // case X: / case 0: / case 1 | 2: / etc.
    AST_DefaultLabel,  // default:
    AST_Labeled,       // case 0: break; / etc.
    // Declaration
    AST_StorageSpecifier,
    AST_FlatTypeSpecifier,
    AST_TypeSpecifier,
    AST_FunctionSpecifier,
    AST_InitDecl,
    AST_StaticAssertDecl,
    AST_Declarations,
    // Statement
    AST_ExprStat,
    AST_CompStat,
    AST_IfStat,
    AST_SwitchStat,
    AST_WhileStat,
    AST_DoWhileStat,
    AST_ForStat,
    AST_GotoStat,
    AST_ContinueStat,
    AST_BreakStat,
    AST_ReturnStat,
    // Expression
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
    AST_ShiftLeftExpr,
    AST_ShiftRightExpr,
    AST_AddExpr,
    AST_SubExpr,
    AST_MultExpr,
    AST_DivExpr,
    AST_ModExpr,
    AST_CastExpr,
    AST_PreIncExpr,
    AST_PreDecExpr,
    AST_RefExpr,
    AST_DerefExpr,
    AST_NegExpr,
    AST_InvExpr,
    AST_NotExpr,
    AST_SizeofExpr,
    AST_AlignofExpr,
    AST_ArrAccessExpr,
    AST_CallExpr,
    AST_AccessExpr,
    AST_DerefAccessExpr,
    AST_PostIncExpr,
    AST_PostDecExpr
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
    CharWide
};

enum str_type { ASCII = 0, U8, U16, U32, Wide };

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

enum type_spec {
    SpecAuto,
    SpecConstexpr,
    SpecExtern,
    SpecRegister,
    SpecStatic,
    SpecThreadLocal,
    SpecTypedef,
    SpecInline,
    SpecNoReturn,
    SpecVoid,
    SpecConst,
    SpecRestrict,
    SpecVolatile,
    SpecAtomicWord,
    SpecChar,
    SpecShort,
    SpecInt,
    SpecLong,
    SpecFloat,
    SpecDouble,
    SpecSigned,
    SpecUnsigned,
    SpecBitInt,
    SpecBool,
    SpecComplex,
    SpecDecimal32,
    SpecDecimal64,
    SpecDecimal128,
    SpecStruct,
    SpecUnion,
    SpecEnum,
    SpecAtomicType,
    SpecTypeof,
    SpecTypeofUnqual,
    SpecAlignas,
};

enum inv_type {
    DidNotMatch = 0,
    BadTokenList,
    BadAttributeSpecifier,
    BadIdLabel,
    BadCaseLabel,
    BadDefaultLabel,
    BadSemicolonDeclaration,
    BadDeclaration,
    BadStaticAssertDeclaration,
    BadStaticAssertStringDeclaration,
    BadPrimaryExpression,
    BadPrimaryExpressionRParen,
    BadExpression,
    BadConditionalExpression,
    BadSemicolonStatement,
    BadCompoundStatement,
    BadUnimplemented
};

// Consider using a flat array and indexes for Ast
// Better performance, easier to free, etc.
typedef struct Ast {
    enum ast_type type;
    Span span;
    union {
        Tree *expr; // General option, contents depend on type
        Lexes *lexes;
        size_t id;
        enum inv_type invalid;
        struct {
            size_t id;
            enum str_type type;
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
        struct {
            Tree *expr;
            size_t id;
        } access;
        struct {
            Tree *expr;
            enum type_spec spec;
        } spec;
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

void print_ast(Ast ast, int depth);
void delete_ast(Ast ast);

#endif // PARSER_H_
