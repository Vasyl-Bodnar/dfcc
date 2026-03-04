/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "parser.h"
#include "lexer.h"
#include "pp.h"
#include "vec.h"
#include <stdio.h>

Ast expression(Parser *parser);
Ast assignment_expression(Parser *parser);
Ast cast_expression(Parser *parser);

// Using a buffer and a stack of save points to backtrack
// Consider packrat as an optimization
Lex next(Parser *parser) {
    if (parser->idx >= parser->ctx->length) {
        Lex lex = pp_lex_next(&parser->pp);
        push_elem_vec(&parser->ctx, &lex);
        parser->idx += 1;
        return lex;
    } else {
        return *(Lex *)at_elem_vec(parser->ctx, parser->idx++);
    }
}

void save_ctx(Parser *parser) {
    push_elem_vec(&parser->idx_stack, &parser->idx);
}

void reset_ctx(Parser *parser) {
    reset_vec(parser->ctx);
    reset_vec(parser->idx_stack);
    parser->idx = 0;
}

void return_ctx(Parser *parser) {
    if (!parser->idx_stack->length) {
        parser->idx = 0;
        return;
    }

    size_t idx = *(size_t *)peek_elem_vec(parser->idx_stack);
    pop_elem_vec(parser->idx_stack);
    parser->idx = idx;
}

void ignore_ctx(Parser *parser) {
    if (parser->idx_stack->length) {
        pop_elem_vec(parser->idx_stack);
    }
}

// TODO: _Generic
// NOTE: We use the assumption that Strings and Constants are in a row
Ast primary_expression(Parser *parser) {
    Lex lex = next(parser);
    switch (lex.type) {
    case LEX_Eof:
        return (Ast){
            .type = AST_Eof,
        };
    case LEX_LParen: {
        Ast ast = expression(parser);
        lex = next(parser);
        if (lex.type == LEX_RParen) {
            return ast;
        } else {
            return (Ast){.type = AST_Invalid,
                         .span = lex.span,
                         .invalid = BadPrimaryExpressionRParen};
        }
    }
    case LEX_Identifier:
        return (Ast){
            .type = AST_Identifier,
            .span = lex.span,
            .id = lex.id,
        };
    case LEX_String:
    case LEX_StringU8:
    case LEX_StringU16:
    case LEX_StringU32:
    case LEX_StringWide:
        return (Ast){
            .type = AST_String,
            .span = lex.span,
            .string = {.type = ASCII + (lex.type - LEX_String), .id = lex.id},
        };
    case LEX_ConstantUnsignedLongLong:
    case LEX_ConstantUnsignedLong:
    case LEX_ConstantUnsignedBitPrecise:
    case LEX_ConstantUnsigned:
    case LEX_ConstantLongLong:
    case LEX_ConstantLong:
    case LEX_ConstantBitPrecise:
    case LEX_Constant:
    case LEX_ConstantFloat:
    case LEX_ConstantDouble:
    case LEX_ConstantLongDouble:
    case LEX_ConstantDecimal32:
    case LEX_ConstantDecimal64:
    case LEX_ConstantDecimal128:
    case LEX_ConstantChar:
    case LEX_ConstantCharU8:
    case LEX_ConstantCharU16:
    case LEX_ConstantCharU32:
    case LEX_ConstantCharWide:
        return (Ast){
            .type = AST_Constant,
            .span = lex.span,
            .constant = {.type = UnsignedLongLong +
                                 (lex.type - LEX_ConstantUnsignedLongLong),
                         .c = lex.constant}};
    default:
        return (Ast){.type = AST_Invalid,
                     .span = lex.span,
                     .invalid = BadPrimaryExpression};
    }
}

Ast compound_literal(Parser *parser) {
    // TODO: type parsing required
    return primary_expression(parser);
}

Ast postfix_expression(Parser *parser) {
    // TODO: Should be primary if not compound
    Ast ast, value = compound_literal(parser);
    if (value.type == AST_Eof) {
        return value;
    }

    int flag = 1;
    while (flag) {
        save_ctx(parser);
        Lex lex = next(parser);
        switch (lex.type) {
        case LEX_LBracket:
            ast = (Ast){.type = AST_ArrAccessExpr,
                        .span = lex.span,
                        .expr = create_tree(2)};
            push_elem_vec(&ast.expr, &value);
            value = expression(parser);
            push_elem_vec(&ast.expr, &value);

            lex = next(parser);
            if (lex.type == LEX_RBracket) {
                ignore_ctx(parser);
                value = ast;
                break;
            }
            delete_tree(ast.expr);
            return_ctx(parser);
            flag = 0;
            break;
        case LEX_LParen:
            save_ctx(parser);
            lex = next(parser);
            if (lex.type == LEX_RParen) {
                ast = (Ast){.type = AST_CallExpr,
                            .span = lex.span,
                            .expr = create_tree(1)};
                push_elem_vec(&ast.expr, &value);
                ignore_ctx(parser);
                ignore_ctx(parser);
                value = ast;
                break;
            }

            return_ctx(parser);
            ast = (Ast){
                .type = AST_CallExpr, .span = lex.span, .expr = create_tree(2)};
            push_elem_vec(&ast.expr, &value);
            value = expression(parser);
            push_elem_vec(&ast.expr, &value);

            lex = next(parser);
            if (lex.type == LEX_RParen) {
                ignore_ctx(parser);
                value = ast;
                break;
            }
            delete_tree(ast.expr);
            return_ctx(parser);
            flag = 0;
            break;
        case LEX_Dot:
            lex = next(parser);
            if (lex.type == LEX_Identifier) {
                ast = (Ast){.type = AST_AccessExpr,
                            .span = lex.span,
                            .access = {.expr = create_tree(1), .id = lex.id}};
                push_elem_vec(&ast.expr, &value);
                ignore_ctx(parser);
                value = ast;
                break;
            }
            return_ctx(parser);
            flag = 0;
            break;
        case LEX_Arrow:
            lex = next(parser);
            if (lex.type == LEX_Identifier) {
                ast = (Ast){.type = AST_DerefAccessExpr,
                            .span = lex.span,
                            .access = {.expr = create_tree(1), .id = lex.id}};
                push_elem_vec(&ast.expr, &value);
                ignore_ctx(parser);
                value = ast;
                break;
            }
            return_ctx(parser);
            flag = 0;
            break;
        case LEX_PlusPlus:
            ast = (Ast){.type = AST_PostIncExpr,
                        .span = lex.span,
                        .expr = create_tree(1)};
            push_elem_vec(&ast.expr, &value);
            ignore_ctx(parser);
            value = ast;
            break;
        case LEX_MinusMinus:
            ast = (Ast){.type = AST_PostDecExpr,
                        .span = lex.span,
                        .expr = create_tree(1)};
            push_elem_vec(&ast.expr, &value);
            ignore_ctx(parser);
            value = ast;
            break;
        default:
            return_ctx(parser);
            flag = 0;
            break;
        }
    }

    return value;
}

Ast unary_expression(Parser *parser) {
    save_ctx(parser);
    Ast ast, value;
    Lex lex = next(parser);
    switch (lex.type) {
    case LEX_PlusPlus:
        ast = (Ast){
            .type = AST_PreIncExpr, .span = lex.span, .expr = create_tree(1)};
        value = unary_expression(parser);
        push_elem_vec(&ast.expr, &value);
        ignore_ctx(parser);
        return ast;
    case LEX_MinusMinus:
        ast = (Ast){
            .type = AST_PreDecExpr, .span = lex.span, .expr = create_tree(1)};
        value = unary_expression(parser);
        push_elem_vec(&ast.expr, &value);
        ignore_ctx(parser);
        return ast;
    case LEX_Et:
        ast = (Ast){
            .type = AST_RefExpr, .span = lex.span, .expr = create_tree(1)};
        value = cast_expression(parser);
        push_elem_vec(&ast.expr, &value);
        ignore_ctx(parser);
        return ast;
    case LEX_Star:
        ast = (Ast){
            .type = AST_DerefExpr, .span = lex.span, .expr = create_tree(1)};
        value = cast_expression(parser);
        push_elem_vec(&ast.expr, &value);
        ignore_ctx(parser);
        return ast;
    case LEX_Plus:
        ignore_ctx(parser);
        return cast_expression(parser);
    case LEX_Minus:
        ast = (Ast){
            .type = AST_NegExpr, .span = lex.span, .expr = create_tree(1)};
        value = cast_expression(parser);
        push_elem_vec(&ast.expr, &value);
        ignore_ctx(parser);
        return ast;
    case LEX_Tilde:
        ast = (Ast){
            .type = AST_InvExpr, .span = lex.span, .expr = create_tree(1)};
        value = cast_expression(parser);
        push_elem_vec(&ast.expr, &value);
        ignore_ctx(parser);
        return ast;
    case LEX_Exclamation:
        ast = (Ast){
            .type = AST_NotExpr, .span = lex.span, .expr = create_tree(1)};
        value = cast_expression(parser);
        push_elem_vec(&ast.expr, &value);
        ignore_ctx(parser);
        return ast;
    case LEX_Keyword:
        if (lex.key == KEY_sizeof) {
            // TODO: Also Parsing types
            ast = (Ast){.type = AST_SizeofExpr,
                        .span = lex.span,
                        .expr = create_tree(1)};
            value = unary_expression(parser);
            push_elem_vec(&ast.expr, &value);
            ignore_ctx(parser);
            return ast;
        } else if (lex.key == KEY_alignof) {
            // TODO: Parsing types
            ignore_ctx(parser);
            return (Ast){.type = AST_Invalid,
                         .span = lex.span,
                         .invalid = BadUnimplemented};
        }
    default:
        return_ctx(parser);
        return postfix_expression(parser);
    }
}

Ast cast_expression(Parser *parser) {
    // TODO: This requires parsing types to parse properly
    return unary_expression(parser);
}

// Another example of heavy duplication with these binary expressions
// However, this is not important enough to bother
Ast mult_expression(Parser *parser) {
    Ast ast = cast_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    while (lex.type == LEX_Star || lex.type == LEX_Slash ||
           lex.type == LEX_Percent) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        enum ast_type type = 0;
        switch (lex.type) {
        case LEX_Star:
            type = AST_MultExpr;
            break;
        case LEX_Slash:
            type = AST_DivExpr;
            break;
        case LEX_Percent:
            type = AST_ModExpr;
            break;
        default:
            // TODO: Delete tree and ast
            // This is impossible however
            return (Ast){.type = AST_Invalid,
                         .span = lex.span,
                         .invalid = BadUnimplemented};
        }

        ast = (Ast){.type = type, .span = lex.span, .expr = tree};

        ignore_ctx(parser);
        Ast right = cast_expression(parser);
        push_elem_vec(&ast.expr, &right);
        save_ctx(parser);
        lex = next(parser);
    }

    return_ctx(parser);
    return ast;
}

Ast add_expression(Parser *parser) {
    Ast ast = mult_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    while (lex.type == LEX_Plus || lex.type == LEX_Minus) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        ast = (Ast){.type = AST_AddExpr + (lex.type - LEX_Plus),
                    .span = lex.span,
                    .expr = tree};

        ignore_ctx(parser);
        Ast right = mult_expression(parser);
        push_elem_vec(&ast.expr, &right);
        save_ctx(parser);
        lex = next(parser);
    }

    return_ctx(parser);
    return ast;
}

Ast shift_expression(Parser *parser) {
    Ast ast = add_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    while (lex.type == LEX_LeftLeft || lex.type == LEX_RightRight) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        ast = (Ast){.type = AST_ShiftLeftExpr + (lex.type - LEX_LeftLeft),
                    .span = lex.span,
                    .expr = tree};

        ignore_ctx(parser);
        Ast right = add_expression(parser);
        push_elem_vec(&ast.expr, &right);
        save_ctx(parser);
        lex = next(parser);
    }

    return_ctx(parser);
    return ast;
}

Ast rel_expression(Parser *parser) {
    Ast ast = shift_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    while (lex.type == LEX_Left || lex.type == LEX_Right ||
           lex.type == LEX_LeftEqual || lex.type == LEX_RightEqual) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        ast = (Ast){.type = AST_LessExpr + (lex.type - LEX_Left),
                    .span = lex.span,
                    .expr = tree};

        ignore_ctx(parser);
        Ast right = shift_expression(parser);
        push_elem_vec(&ast.expr, &right);
        save_ctx(parser);
        lex = next(parser);
    }

    return_ctx(parser);
    return ast;
}

Ast equal_expression(Parser *parser) {
    Ast ast = rel_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    while (lex.type == LEX_EqualEqual || lex.type == LEX_ExclamationEqual) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        ast = (Ast){.type = AST_EqualExpr + (lex.type - LEX_EqualEqual),
                    .span = lex.span,
                    .expr = tree};

        ignore_ctx(parser);
        Ast right = rel_expression(parser);
        push_elem_vec(&ast.expr, &right);
        save_ctx(parser);
        lex = next(parser);
    }

    return_ctx(parser);
    return ast;
}

// NOTE: We just duplicate these rules.
// They are very near identical, so it is possible to macro them.
// However, it is not necessary, for now.
Ast and_expression(Parser *parser) {
    Ast ast = equal_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_Et) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        do {
            ignore_ctx(parser);
            ast = equal_expression(parser);
            push_elem_vec(&tree, &ast);
            save_ctx(parser);
            lex = next(parser);
        } while (lex.type == LEX_Et);

        return_ctx(parser);
        return (Ast){.type = AST_AndExpr, .span = lex.span, .expr = tree};
    }

    return_ctx(parser);
    return ast;
}

Ast exclor_expression(Parser *parser) {
    Ast ast = and_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_Caret) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        do {
            ignore_ctx(parser);
            ast = and_expression(parser);
            push_elem_vec(&tree, &ast);
            save_ctx(parser);
            lex = next(parser);
        } while (lex.type == LEX_Caret);

        return_ctx(parser);
        return (Ast){.type = AST_ExclOrExpr, .span = lex.span, .expr = tree};
    }

    return_ctx(parser);
    return ast;
}

Ast inclor_expression(Parser *parser) {
    Ast ast = exclor_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_Pipe) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        do {
            ignore_ctx(parser);
            ast = exclor_expression(parser);
            push_elem_vec(&tree, &ast);
            save_ctx(parser);
            lex = next(parser);
        } while (lex.type == LEX_Pipe);

        return_ctx(parser);
        return (Ast){.type = AST_InclOrExpr, .span = lex.span, .expr = tree};
    }

    return_ctx(parser);
    return ast;
}

Ast logand_expression(Parser *parser) {
    Ast ast = inclor_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_EtEt) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        do {
            ignore_ctx(parser);
            ast = inclor_expression(parser);
            push_elem_vec(&tree, &ast);
            save_ctx(parser);
            lex = next(parser);
        } while (lex.type == LEX_EtEt);

        return_ctx(parser);
        return (Ast){.type = AST_LogAndExpr, .span = lex.span, .expr = tree};
    }

    return_ctx(parser);
    return ast;
}

Ast logor_expression(Parser *parser) {
    Ast ast = logand_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_PipePipe) {
        Tree *tree = create_tree(2);
        push_elem_vec(&tree, &ast);

        do {
            ignore_ctx(parser);
            ast = logand_expression(parser);
            push_elem_vec(&tree, &ast);
            save_ctx(parser);
            lex = next(parser);
        } while (lex.type == LEX_PipePipe);

        return_ctx(parser);
        return (Ast){.type = AST_LogOrExpr, .span = lex.span, .expr = tree};
    }

    return_ctx(parser);
    return ast;
}

Ast conditional_expression(Parser *parser) {
    Ast ast = logor_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_Question) {
        Tree *tree = create_tree(3);
        push_elem_vec(&tree, &ast);

        ast = expression(parser);
        push_elem_vec(&tree, &ast);

        Lex lex = next(parser);
        if (lex.type == LEX_Colon) {
            ast = conditional_expression(parser);
            push_elem_vec(&tree, &ast);

            ignore_ctx(parser);
            return (Ast){.type = AST_CondExpr, .span = lex.span, .expr = tree};
        } else {
            ignore_ctx(parser);
            return (Ast){.type = AST_Invalid,
                         .span = lex.span,
                         .invalid = BadConditionalExpression};
        }
    }

    return_ctx(parser);
    return ast;
}

Ast assignment_expression(Parser *parser) {
    save_ctx(parser);
    Ast ast = unary_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    enum assign_op op;
    Lex lex = next(parser);
    switch (lex.type) {
    case LEX_Equal:
    case LEX_StarEqual:
    case LEX_SlashEqual:
    case LEX_PercentEqual:
    case LEX_PlusEqual:
    case LEX_MinusEqual:
    case LEX_LeftLeftEqual:
    case LEX_RightRightEqual:
    case LEX_EtEqual:
    case LEX_CaretEqual:
    case LEX_PipeEqual:
        op = Assign + (lex.type - LEX_Equal);
        break;
    default:
        return_ctx(parser);
        return conditional_expression(parser);
    }
    Tree *tree = create_tree(2);
    push_elem_vec(&tree, &ast);
    ast = assignment_expression(parser);
    push_elem_vec(&tree, &ast);

    ignore_ctx(parser);
    return (Ast){.type = AST_AssignExpr,
                 .span = lex.span,
                 .assign = {.assigns = tree, .op = op}};
}

Ast expression(Parser *parser) {
    Ast ast = assignment_expression(parser);
    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_Comma) {
        ignore_ctx(parser);
        Tree *assignments = create_tree(2);
        push_elem_vec(&assignments, &ast);
        while (1) {
            Ast ast = assignment_expression(parser);
            push_elem_vec(&assignments, &ast);
            save_ctx(parser);
            Lex lex = next(parser);
            if (lex.type != LEX_Comma) {
                return_ctx(parser);
                return (Ast){
                    .type = AST_Expr, .span = lex.span, .expr = assignments};
            }
            ignore_ctx(parser);
        }
    }
    return_ctx(parser);
    return ast;
}

Ast parse(Parser *parser) {
    Ast ast = expression(parser);
    reset_ctx(parser);
    return ast;
}

// Basically a copy of pp
Parser *create_parser(String *file_path) {
    Parser *parser = malloc(sizeof(*parser));
    parser->ctx = create_lexes(8);
    parser->idx_stack = create_vec(8, sizeof(size_t));
    parser->idx = 0;
    parser->pp.incl_table = create_vec(8, sizeof(IncludeResource));
    parser->pp.incl_stack = create_vec(8, sizeof(size_t));
    parser->pp.macro_table = create_dht(8, sizeof(size_t), sizeof(DefineMacro));
    parser->pp.id_table = create_ids(8);
    parser->pp.macro_if_depth = 0;

    include_file(&parser->pp, file_path);

    return parser;
}

void print_parser(Parser *parser) {
    printf("(PARSER: idx: %zu ctx-stack:\n", parser->idx);
    for (size_t i = 0; i < parser->idx_stack->length; i++) {
        printf(" %zu\n", *(size_t *)at_elem_vec(parser->idx_stack, i));
    }
    printf("context:\n");
    print_lexes(parser->ctx);
    printf("pp:\n");
    print_pp(&parser->pp);
    printf(")\n'");
}

// Basically a copy of pp
void delete_parser(Parser *parser) {
    for (size_t i = 0; i < parser->pp.incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(parser->pp.incl_table, i);
        if (resc->type == IncludeFile) {
            delete_str(resc->path);
            free(resc->stream.start);
        } else if (resc->type == IncludeMacro) {
            if (resc->args) {
                delete_args(resc->args);
            }
        }
    }
    delete_vec(parser->pp.incl_table);
    delete_vec(parser->pp.incl_stack);
    delete_vec(parser->pp.id_table);
    Entry entry;
    size_t idx = 0;
    while ((entry = next_elem_dht(parser->pp.macro_table, &idx)).key) {
        clean_macro(entry.value);
    }
    delete_dht(parser->pp.macro_table);

    delete_vec(parser->ctx);
    free(parser);
}

Tree *create_tree(size_t capacity) { return create_vec(capacity, sizeof(Ast)); }

void print_ast(Ast ast, int depth) {
    switch (ast.type) {
    case AST_Invalid:
        printf("%*c:Ast Error %d: [%p, %zu, %zu, %zu]\n", depth, ' ',
               ast.invalid, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        break;
    case AST_Eof:
        printf("%*c:Ast Eof: [%p, %zu, %zu, %zu]\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_Identifier:
        printf("%*c:Id %zu: [%p, %zu, %zu, %zu]\n", depth, ' ', ast.id,
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_Constant:
        switch (ast.constant.type) {
        case UnsignedLongLong:
        case UnsignedLong:
        case UnsignedBitPrecise:
        case Unsigned:
        case LongLong:
        case Long:
        case BitPrecise:
        case Int:
        case Decimal32:
        case Decimal64:
        case Decimal128:
            printf("%*c:Constant%d val: %zu: [%p, %zu, %zu, %zu]\n", depth, ' ',
                   ast.constant.type, ast.constant.c, ast.span.start,
                   ast.span.len, ast.span.row, ast.span.col);
            break;
        case Char:
        case CharU8:
        case CharU16:
        case CharU32:
        case CharWide:
            printf("%*c:ConstantChar%d val: %zu: [%p, %zu, %zu, %zu]\n", depth,
                   ' ', ast.constant.type, ast.constant.c, ast.span.start,
                   ast.span.len, ast.span.row, ast.span.col);
            break;
        case Float:
        case Double:
        case LongDouble:
            printf("%*c:ConstantFloat%d val: %f: [%p, %zu, %zu, %zu]\n", depth,
                   ' ', ast.constant.type, ast.constant.d, ast.span.start,
                   ast.span.len, ast.span.row, ast.span.col);
            break;
        }
        break;
    case AST_String:
        printf("%*c:String%d id: %zu: [%p, %zu, %zu, %zu]\n", depth, ' ',
               ast.string.type, ast.string.id, ast.span.start, ast.span.len,
               ast.span.row, ast.span.col);
        break;
    case AST_Expr:
        printf("%*c:Expr: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_AssignExpr:
        printf("%*c:Assign%d: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.assign.op, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        print_tree(ast.assign.assigns, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_CondExpr:
        printf("%*c:Condition: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_LogOrExpr:
        printf("%*c:LogicalOr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_LogAndExpr:
        printf("%*c:LogicalAnd: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_InclOrExpr:
        printf("%*c:InclusiveOr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_ExclOrExpr:
        printf("%*c:ExclusiveOr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_AndExpr:
        printf("%*c:And: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_EqualExpr:
        printf("%*c:Equal\n", depth, ' ');
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_NotEqualExpr:
        printf("%*c:NotEqual: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_LessExpr:
        printf("%*c:Less: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_GreaterExpr:
        printf("%*c:Greater: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_LessEqExpr:
        printf("%*c:LessEqual: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_GreaterEqExpr:
        printf("%*c:GreaterEqual: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_ShiftLeftExpr:
        printf("%*c:ShiftLeft: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_ShiftRightExpr:
        printf("%*c:ShiftRight: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_AddExpr:
        printf("%*c:Add: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_SubExpr:
        printf("%*c:Sub: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_MultExpr:
        printf("%*c:Mult: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_DivExpr:
        printf("%*c:Div: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_ModExpr:
        printf("%*c:Mod: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_CastExpr:
        printf("%*c:Cast: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        // TODO: This should be a type
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_PreIncExpr:
        printf("%*c:PreInc: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_PreDecExpr:
        printf("%*c:PreDec: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_RefExpr:
        printf("%*c:RefExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_DerefExpr:
        printf("%*c:DerefExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_NegExpr:
        printf("%*c:NegExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_InvExpr:
        printf("%*c:InvExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_NotExpr:
        printf("%*c:NotExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_SizeofExpr:
        printf("%*c:SizeofExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_AlignofExpr:
        printf("%*c:AlignofExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_ArrAccessExpr:
        printf("%*c:ArrAccess: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_CallExpr:
        printf("%*c:Call: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_AccessExpr:
        printf("%*c:Access %zu: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.access.id, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        print_tree(ast.access.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_DerefAccessExpr:
        printf("%*c:DerefAccess %zu: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.access.id, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        print_tree(ast.access.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_PostIncExpr:
        printf("%*c:PostInc: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_PostDecExpr:
        printf("%*c:PostDec: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    }
}

void print_tree(Tree *tree, int depth) {
    for (uint64_t i = 0; i < tree->length; i++) {
        print_ast(*(Ast *)at_elem_vec((Tree *)tree, i), depth);
    }
}

// TODO: Delete Subtrees
void delete_tree(Tree *tree) { return delete_vec(tree); }
