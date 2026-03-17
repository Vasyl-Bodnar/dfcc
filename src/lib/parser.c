/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "parser.h"
#include "lexer.h"
#include "pp.h"
#include "vec.h"
#include <stdio.h>

// NOTE: Could use more parsing abstractions (e.g. optional, check this token)
// Some clean up might be required of more messy and fragile parts
// Certain tricks are not used consistenly (e.g. "this rule failed")

Ast statement(Parser *parser);
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
            delete_ast(ast);
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
            delete_tree(tree);
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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
            delete_tree(tree);
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
    if (ast.type == AST_Eof || ast.type == AST_EofInvalid) {
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

Ast token_list(Parser *parser) {
    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_LParen) {
        ignore_ctx(parser);

        Ast ast = {
            .type = AST_LexList, .span = lex.span, .lexes = create_lexes(1)};
        size_t paren_depth = 1;
        size_t bracket_depth = 0;
        size_t squigly_depth = 0;
        while (paren_depth) {
            lex = next(parser);
            switch (lex.type) {
            case LEX_Eof:
                return (Ast){.type = AST_EofInvalid,
                             .span = lex.span,
                             .invalid = BadTokenList};
            case LEX_LParen:
                paren_depth += 1;
                break;
            case LEX_RParen:
                paren_depth -= 1;
                break;
            case LEX_LBracket:
                bracket_depth += 1;
                break;
            case LEX_RBracket:
                bracket_depth -= 1;
                break;
            case LEX_LSquigly:
                squigly_depth += 1;
                break;
            case LEX_RSquigly:
                squigly_depth -= 1;
                break;
            default:
                push_elem_vec(&ast.lexes, &lex);
                break;
            }
        }

        if (bracket_depth || squigly_depth) {
            delete_vec(ast.lexes);
            return (Ast){
                .type = AST_Invalid, .span = lex.span, .invalid = BadTokenList};
        }

        return ast;
    }
    return_ctx(parser);
    return (Ast){.type = AST_LexList, .span = lex.span, .lexes = 0};
}

// NOTE: Assumes that first [[ were already parsed
Ast attribute_specifier(Parser *parser) {
    Ast ast = {.type = AST_AttributeList, .expr = create_tree(1)};
    int flag = 1;
    while (flag) {
        Lex lex = next(parser);
        switch (lex.type) {
        case LEX_Eof:
            delete_ast(ast);
            return (Ast){.type = AST_EofInvalid,
                         .span = lex.span,
                         .invalid = BadAttributeSpecifier};
        case LEX_RBracket:
            lex = next(parser);
            if (lex.type == LEX_RBracket) {
                save_ctx(parser);
                lex = next(parser);
                if (lex.type == LEX_LBracket) {
                    lex = next(parser);
                    if (lex.type == LEX_LBracket) {
                        ignore_ctx(parser);
                    } else {
                        return_ctx(parser);
                        flag = 0;
                        ast.span = lex.span;
                    }
                } else {
                    return_ctx(parser);
                    flag = 0;
                    ast.span = lex.span;
                }
            } else {
                delete_ast(ast);
                return (Ast){.type = AST_Invalid,
                             .span = lex.span,
                             .invalid = BadAttributeSpecifier};
            }
            break;
        case LEX_Comma:
            break;
        case LEX_Identifier: {
            Ast attr = {.type = AST_Attribute,
                        .span = lex.span,
                        .expr = create_tree(1)};
            Ast id = {.type = AST_Identifier, .span = lex.span, .id = lex.id};
            push_elem_vec(&attr.expr, &id);
            save_ctx(parser);
            lex = next(parser);
            if (lex.type == LEX_ColonColon) {
                ignore_ctx(parser);
                lex = next(parser);
                if (lex.type == LEX_Identifier) {
                    id = (Ast){
                        .type = AST_Identifier, .span = lex.span, .id = lex.id};
                    push_elem_vec(&attr.expr, &id);
                    Ast list = token_list(parser);
                    if (list.lexes) {
                        push_elem_vec(&attr.expr, &list);
                        push_elem_vec(&ast.expr, &attr);
                    } else {
                        push_elem_vec(&ast.expr, &attr);
                    }
                    break;
                }
            }
            return_ctx(parser);
            Ast list = token_list(parser);
            if (list.lexes) {
                push_elem_vec(&attr.expr, &list);
                push_elem_vec(&ast.expr, &attr);
            } else {
                push_elem_vec(&ast.expr, &attr);
            }
            break;
        }
        default:
            delete_ast(ast);
            return (Ast){.type = AST_Invalid,
                         .span = lex.span,
                         .invalid = BadAttributeSpecifier};
        }
    }
    return ast;
}

// Check for [[ and backtrack if not
int double_lbracket(Parser *parser) {
    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_LBracket) {
        lex = next(parser);
        if (lex.type == LEX_LBracket) {
            ignore_ctx(parser);
            return 1;
        }
    }
    return_ctx(parser);
    return 0;
}

Ast label(Parser *parser) {
    save_ctx(parser);
    int attr_flag = double_lbracket(parser);
    Ast attr, collect;

    if (attr_flag) {
        attr = (Ast){.type = AST_Attributed, .expr = create_tree(2)};
        Ast att = attribute_specifier(parser);
        attr.span = att.span;
        push_elem_vec(&attr.expr, &att);
    }

    Lex oth_lex, lex = next(parser);
    switch (lex.type) {
    case LEX_Keyword:
        if (lex.key == KEY_case) {
            Ast constant = conditional_expression(parser); // constant
            lex = next(parser);
            if (lex.type == LEX_Colon) {
                collect = (Ast){.type = AST_CaseLabel,
                                .span = lex.span,
                                .expr = create_tree(1)};
                push_elem_vec(&collect.expr, &constant);
            } else {
                collect = (Ast){.type = AST_Invalid,
                                .span = lex.span,
                                .invalid = BadCaseLabel};
            }
            break;
        } else if (lex.key == KEY_default) {
            lex = next(parser);
            if (lex.type == LEX_Colon) {
                collect = (Ast){.type = AST_DefaultLabel, .span = lex.span};
            } else {
                collect = (Ast){.type = AST_Invalid,
                                .span = lex.span,
                                .invalid = BadDefaultLabel};
            }
            break;
        }
    case LEX_Identifier:
        oth_lex = next(parser);
        if (oth_lex.type == LEX_Colon) {
            collect =
                (Ast){.type = AST_IdLabel, .span = lex.span, .id = lex.id};
            break;
        }
    default:
        if (attr_flag) {
            delete_ast(attr);
        }
        return_ctx(parser);
        return (Ast){
            .type = AST_Invalid, .span = lex.span, .invalid = DidNotMatch};
    }

    if (attr_flag) {
        push_elem_vec(&attr.expr, &collect);
        ignore_ctx(parser);
        return attr;
    } else {
        ignore_ctx(parser);
        return collect;
    }
}

Ast expression_statement(Parser *parser) {
    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_Semicolon) {
        ignore_ctx(parser);
        return (Ast){.type = AST_ExprStat, .span = lex.span, .expr = 0};
    }
    return_ctx(parser);

    int attr_flag = double_lbracket(parser);
    Ast attr;
    if (attr_flag) {
        attr = (Ast){.type = AST_Attributed, .expr = create_tree(2)};
        Ast att = attribute_specifier(parser);
        attr.span = att.span;
        push_elem_vec(&attr.expr, &att);
    }

    Ast ast = {.type = AST_ExprStat, .span = lex.span, .expr = create_tree(1)};

    Ast value = expression(parser);
    if (value.type == AST_Eof || value.type == AST_EofInvalid) {
        if (attr_flag) {
            delete_ast(attr);
        }
        delete_ast(ast);
        return value;
    }

    push_elem_vec(&ast.expr, &value);

    lex = next(parser);
    if (lex.type == LEX_Semicolon) {
        if (attr_flag) {
            push_elem_vec(&attr.expr, &ast);
            return attr;
        }
        return ast;
    }

    if (attr_flag) {
        delete_ast(attr);
    }
    delete_ast(ast);
    return (Ast){.type = AST_Invalid,
                 .span = lex.span,
                 .invalid = BadSemicolonStatement};
}

Ast unlabeled_statement(Parser *parser) {
    save_ctx(parser);

    int attr_flag = double_lbracket(parser);
    Ast attr, collect;

    if (attr_flag) {
        attr = (Ast){.type = AST_Attributed, .expr = create_tree(2)};
        Ast att = attribute_specifier(parser);
        attr.span = att.span;
        push_elem_vec(&attr.expr, &att);
    }

    Lex lex = next(parser);
    if (lex.type == LEX_LSquigly) {
        save_ctx(parser);
        lex = next(parser);
        if (lex.type == LEX_RSquigly) {
            ignore_ctx(parser);
            ignore_ctx(parser);
            collect = (Ast){.type = AST_CompStat, .span = lex.span, .expr = 0};
            goto Check_Attribute_Ret;
        }
        return_ctx(parser);

        Ast value, ast = {.type = AST_CompStat,
                          .span = lex.span,
                          .expr = create_tree(1)};

        while (1) {
            // TODO: Declaration
            value = label(parser);
            if (value.type == AST_Invalid && !value.invalid) {
                value = unlabeled_statement(parser);
            }
            if (value.type == AST_Eof || value.type == AST_EofInvalid) {
                if (attr_flag) {
                    delete_ast(attr);
                }
                delete_ast(ast);
                delete_ast(value);
                ignore_ctx(parser);
                return (Ast){.type = AST_EofInvalid,
                             .span = lex.span,
                             .invalid = BadCompoundStatement};
            }
            push_elem_vec(&ast.expr, &value);

            save_ctx(parser);
            lex = next(parser);
            if (lex.type == LEX_RSquigly) {
                ignore_ctx(parser);
                ignore_ctx(parser);
                collect = ast;
                goto Check_Attribute_Ret;
            }
            return_ctx(parser);
        }
    }

    if (lex.type == LEX_Keyword) {
        switch (lex.key) {
        case KEY_if:
            lex = next(parser);
            if (lex.type == LEX_LParen) {
                Ast ast = {.type = AST_IfStat,
                           .span = lex.span,
                           .expr = create_tree(2)};
                Ast expr = expression(parser);
                push_elem_vec(&ast.expr, &expr);
                lex = next(parser);
                if (lex.type == LEX_RParen) {
                    // TODO: Else works like intended?
                    Ast block = statement(parser);
                    push_elem_vec(&ast.expr, &block);
                    save_ctx(parser);
                    lex = next(parser);
                    if (lex.type == LEX_Keyword && lex.key == KEY_else) {
                        block = statement(parser);
                        push_elem_vec(&ast.expr, &block);
                        ignore_ctx(parser);
                    } else {
                        return_ctx(parser);
                    }
                    ignore_ctx(parser);
                    collect = ast;
                    goto Check_Attribute_Ret;
                }
                delete_ast(ast);
            }
            break;
        case KEY_switch:
            lex = next(parser);
            if (lex.type == LEX_LParen) {
                Ast ast = {.type = AST_SwitchStat,
                           .span = lex.span,
                           .expr = create_tree(2)};
                Ast expr = expression(parser);
                push_elem_vec(&ast.expr, &expr);
                lex = next(parser);
                if (lex.type == LEX_RParen) {
                    Ast block = statement(parser);
                    push_elem_vec(&ast.expr, &block);

                    ignore_ctx(parser);
                    collect = ast;
                    goto Check_Attribute_Ret;
                }
                delete_ast(ast);
            }
            break;
        case KEY_while:
            lex = next(parser);
            if (lex.type == LEX_LParen) {
                Ast ast = {.type = AST_WhileStat,
                           .span = lex.span,
                           .expr = create_tree(2)};
                Ast expr = expression(parser);
                push_elem_vec(&ast.expr, &expr);
                lex = next(parser);
                if (lex.type == LEX_RParen) {
                    Ast block = statement(parser);
                    push_elem_vec(&ast.expr, &block);

                    ignore_ctx(parser);
                    collect = ast;
                    goto Check_Attribute_Ret;
                }
                delete_ast(ast);
            }
            break;
        case KEY_do: {
            Ast block = statement(parser);
            lex = next(parser);
            if (lex.type == LEX_Keyword && lex.key == KEY_while) {
                Ast ast = {.type = AST_DoWhileStat,
                           .span = lex.span,
                           .expr = create_tree(2)};
                push_elem_vec(&ast.expr, &block);
                lex = next(parser);
                if (lex.type == LEX_LParen) {
                    Ast expr = expression(parser);
                    push_elem_vec(&ast.expr, &expr);
                    lex = next(parser);
                    if (lex.type == LEX_RParen) {
                        lex = next(parser);
                        if (lex.type == LEX_Semicolon) {
                            ignore_ctx(parser);
                            collect = ast;
                            goto Check_Attribute_Ret;
                        }
                    }
                }
                delete_ast(ast);
            }
            break;
        }
        case KEY_for:
            // TODO: Handle declaration kind too e.g. int i = 0;
            lex = next(parser);
            if (lex.type == LEX_LParen) {
                Ast expr, ast = {.type = AST_ForStat,
                                 .span = lex.span,
                                 .expr = create_tree(4)};
                for (int i = 0; i < 3; i++) {
                    save_ctx(parser);
                    lex = next(parser);
                    if (lex.type == LEX_Semicolon) {
                        ignore_ctx(parser);
                    } else {
                        return_ctx(parser);
                        expr = expression(parser);
                        push_elem_vec(&ast.expr, &expr);
                        lex = next(parser);
                        if (i == 2 && lex.type == LEX_RParen) {
                            Ast block = statement(parser);
                            push_elem_vec(&ast.expr, &block);

                            ignore_ctx(parser);
                            collect = ast;
                            goto Check_Attribute_Ret;
                        } else if (lex.type != LEX_Semicolon) {
                            break;
                        }
                    }
                }

                delete_ast(ast);
            }
            break;
        case KEY_goto:
            lex = next(parser);
            if (lex.type == LEX_Identifier) {
                Lex sem = next(parser);
                if (sem.type == LEX_Semicolon) {
                    ignore_ctx(parser);
                    collect = (Ast){
                        .type = AST_GotoStat, .span = lex.span, .id = lex.id};
                    goto Check_Attribute_Ret;
                }
            }
            break;
        case KEY_continue:
            lex = next(parser);
            if (lex.type == LEX_Semicolon) {
                ignore_ctx(parser);
                collect = (Ast){.type = AST_ContinueStat, .span = lex.span};
                goto Check_Attribute_Ret;
            }
            break;
        case KEY_break:
            lex = next(parser);
            if (lex.type == LEX_Semicolon) {
                ignore_ctx(parser);
                collect = (Ast){.type = AST_BreakStat, .span = lex.span};
                goto Check_Attribute_Ret;
            }
            break;
        case KEY_return:
            save_ctx(parser);
            lex = next(parser);
            if (lex.type == LEX_Semicolon) {
                ignore_ctx(parser);
                ignore_ctx(parser);
                collect =
                    (Ast){.type = AST_ReturnStat, .span = lex.span, .expr = 0};
                goto Check_Attribute_Ret;
            }
            return_ctx(parser);

            Tree *tree = create_tree(1);
            Ast expr = expression(parser);
            push_elem_vec(&tree, &expr);

            lex = next(parser);
            if (lex.type == LEX_Semicolon) {
                ignore_ctx(parser);
                collect = (Ast){
                    .type = AST_ReturnStat, .span = lex.span, .expr = tree};
                goto Check_Attribute_Ret;
            }
            delete_tree(tree);
            break;
        default:
            break;
        }
    }

    if (attr_flag) {
        delete_ast(attr);
    }
    return_ctx(parser);
    return expression_statement(parser);
Check_Attribute_Ret:
    if (attr_flag) {
        push_elem_vec(&attr.expr, &collect);
        return attr;
    } else {
        return collect;
    }
}

Ast statement(Parser *parser) {
    Ast lab = label(parser);
    if (lab.type == AST_Invalid && !lab.invalid) {
        return unlabeled_statement(parser);
    } else {
        Ast labeled = {
            .type = AST_Labeled, .span = lab.span, .expr = create_tree(2)};
        push_elem_vec(&labeled.expr, &lab);
        Ast stat = statement(parser);
        push_elem_vec(&labeled.expr, &stat);
        return labeled;
    }
}

Ast declaration(Parser *parser) {
    save_ctx(parser);
    Lex lex = next(parser);
    if (lex.type == LEX_Eof) {
        return (Ast){.type = AST_Eof, .span = lex.span};
    }

    if (lex.type == LEX_Keyword && lex.key == KEY_static_assert) {
        ignore_ctx(parser);
        Ast ast = {.type = AST_StaticAssertDecl,
                   .span = lex.span,
                   .expr = create_tree(2)};
        lex = next(parser);
        if (lex.type == LEX_LParen) {
            Ast expr = conditional_expression(parser); // constant-expression
            push_elem_vec(&ast.expr, &expr);
            lex = next(parser);
            if (lex.type == LEX_Comma) {
                expr = primary_expression(parser);
                if (expr.type != AST_String) {
                    delete_ast(ast);
                    return (Ast){.type = AST_Invalid,
                                 .span = expr.span,
                                 .invalid = BadStaticAssertStringDeclaration};
                }
                push_elem_vec(&ast.expr, &expr);
                lex = next(parser);
            }
            if (lex.type == LEX_RParen) {
                lex = next(parser);
                if (lex.type == LEX_Semicolon) {
                    return ast;
                }
            }
        }
        delete_ast(ast);
        return (Ast){.type = AST_Invalid,
                     .span = ast.span, // NOTE: It is valid to use ast's span as
                                       // it is stack allocated
                     .invalid = BadStaticAssertDeclaration};
    }
    return_ctx(parser);

    int attr_flag = double_lbracket(parser);
    Ast attr, collect;

    if (attr_flag) {
        attr = (Ast){.type = AST_Attributed, .expr = create_tree(2)};
        Ast att = attribute_specifier(parser);
        attr.span = att.span;
        push_elem_vec(&attr.expr, &att);
    }

    save_ctx(parser);
    lex = next(parser);
    if (lex.type == LEX_Semicolon && attr_flag) {
        ignore_ctx(parser);
        return attr;
    }
    return_ctx(parser);

    // Dealing with types then
    collect =
        (Ast){.type = AST_InitDecl, .span = lex.span, .expr = create_tree(2)};

    Ast ast, specs = {.type = AST_Specifiers,
                      .span = lex.span,
                      .expr = create_tree(1)};
    do {
        lex = next(parser);
        switch (lex.type) {
        case LEX_Keyword:
            switch (lex.key) {
            case KEY_auto:
                ast = (Ast){.type = AST_StorageSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecAuto}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_constexpr:
                ast = (Ast){.type = AST_StorageSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecConstexpr}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_extern:
                ast = (Ast){.type = AST_StorageSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecExtern}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_register:
                ast = (Ast){.type = AST_StorageSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecRegister}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_static:
                ast = (Ast){.type = AST_StorageSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecStatic}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_thread_local:
                ast = (Ast){.type = AST_StorageSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecThreadLocal}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_typedef:
                ast = (Ast){.type = AST_StorageSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecTypedef}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_void:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecVoid}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_char:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecChar}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_short:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecShort}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_int:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecInt}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_long:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecLong}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_float:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecFloat}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_double:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecDouble}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_signed:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecSigned}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_unsigned:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecUnsigned}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY__BitInt: // TODO: Special
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecBitInt}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_bool:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecBool}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY__Complex:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecComplex}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY__Decimal32:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecDecimal32}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY__Decimal64:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecDecimal64}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY__Decimal128:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecDecimal128}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_struct: // TODO: Special
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecStruct}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_union: // TODO: Special
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecUnion}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_enum: // TODO: Special
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecEnum}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY__Atomic: // TODO: with or without (name)
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecAtomicWord}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_typeof: // TODO: Special
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecTypeof}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_typeof_unqual: // TODO: Special
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecTypeofUnqual}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_const:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecConst}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_restrict:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecRestrict}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_volatile:
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecVolatile}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_inline:
                ast = (Ast){.type = AST_FunctionSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecInline}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY__Noreturn:
                ast = (Ast){.type = AST_FunctionSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecNoReturn}};
                push_elem_vec(&specs.expr, &ast);
                break;
            case KEY_alignas: // TODO: Special
                ast = (Ast){.type = AST_FlatTypeSpecifier,
                            .span = lex.span,
                            .spec = {0, SpecAlignas}};
                push_elem_vec(&specs.expr, &ast);
                break;
            default:
                ast = (Ast){.type = AST_Invalid,
                            .span = lex.span,
                            .invalid = DidNotMatch};
                break;
            }
            break;
        // case LEX_Identifier: // NOTE: Non-context-free parsing here
        case LEX_Eof:
            if (attr_flag) {
                delete_ast(attr);
                delete_ast(specs);
                delete_ast(collect);
                return (Ast){.type = AST_EofInvalid,
                             .span = lex.span,
                             .invalid = BadDeclaration};
            } else if (collect.expr->length) {
                delete_ast(specs);
                delete_ast(collect);
                return (Ast){.type = AST_EofInvalid,
                             .span = lex.span,
                             .invalid = BadDeclaration};
            } else {
                delete_ast(specs);
                delete_ast(collect);
                return (Ast){.type = AST_Eof, .span = lex.span};
            }
        default:
            ast = (Ast){
                .type = AST_Invalid, .span = lex.span, .invalid = DidNotMatch};
            break;
        }
    } while (ast.type != AST_Invalid);

    push_elem_vec(&collect.expr, &specs);

    // init-declarator is not optional with attributes
    if (lex.type == LEX_Semicolon && !attr_flag) {
        return collect;
    }

    if (lex.type == LEX_Semicolon) {
        if (attr_flag) {
            push_elem_vec(&attr.expr, &collect);
            return attr;
        }
        return collect;
    }

    if (attr_flag) {
        delete_ast(attr);
    }
    delete_ast(collect);
    return (Ast){.type = AST_Invalid,
                 .span = lex.span,
                 .invalid = BadSemicolonDeclaration};
}

Ast declarations(Parser *parser) {
    Ast decl, ast = {.type = AST_Declarations, .expr = create_tree(8)};
    do {
        decl = declaration(parser);
        push_elem_vec(&ast.expr, &decl);
    } while (decl.type != AST_Eof && decl.type != AST_EofInvalid);
    ast.span = decl.span;
    return ast;
}

// TODO: Have to be more careful with how Ast are freed throughout
Ast parse(Parser *parser) {
    Ast ast = declarations(parser);
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
    print_lexes(parser->ctx, 0);
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

// TODO: Fix spans, currently we just grab the last lex we used for the Ast
void print_ast(Ast ast, int depth) {
    switch (ast.type) {
    case AST_EofInvalid:
        printf("%*c:Ast Eof Error %d: [%p, %zu, %zu, %zu]\n", depth, ' ',
               ast.invalid, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        return;
    case AST_Invalid:
        printf("%*c:Ast Error %d: [%p, %zu, %zu, %zu]\n", depth, ' ',
               ast.invalid, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        return;
    case AST_Eof:
        printf("%*c:Ast Eof: [%p, %zu, %zu, %zu]\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        return;
    case AST_LexList:
        printf("%*c:LexList: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        if (ast.expr) {
            print_lexes(ast.lexes, depth + 2);
        }
        printf("%*c::\n", depth, ' ');
        return;
    case AST_Attribute:
        printf("%*c:Attribute: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_AttributeList:
        printf("%*c:AttributeList: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        if (ast.expr) {
            print_tree(ast.expr, depth + 2);
        }
        printf("%*c::\n", depth, ' ');
        return;
    case AST_Attributed:
        printf("%*c:Attributed: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_IdLabel:
        printf("%*c:IdLabel %zu: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.id,
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        return;
    case AST_CaseLabel:
        printf("%*c:CaseLabel: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_DefaultLabel:
        printf("%*c:DefaultLabel: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        return;
    case AST_Labeled:
        printf("%*c:Labeled: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_StorageSpecifier:
        printf("%*c:StorageSpecifier %d: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.spec.spec, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        return;
    case AST_FlatTypeSpecifier:
        printf("%*c:FlatTypeSpecifier %d: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.spec.spec, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        return;
    case AST_TypeSpecifier:
        printf("%*c:TypeSpecifier %d: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.spec.spec, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        break;
    case AST_FunctionSpecifier:
        printf("%*c:FunctionSpecifier %d: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.spec.spec, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        return;
    case AST_Specifiers:
        printf("%*c:Specifiers: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_InitDecl:
        printf("%*c:InitDecl: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_StaticAssertDecl:
        printf("%*c:StaticAssert: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_Declarations:
        printf("%*c:Declarations: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_ExprStat:
        printf("%*c:Stat; [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        if (ast.expr) {
            print_tree(ast.expr, depth + 2);
        }
        printf("%*c::\n", depth, ' ');
        return;
    case AST_CompStat:
        printf("%*c:Compound; [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        if (ast.expr) {
            print_tree(ast.expr, depth + 2);
        }
        printf("%*c::\n", depth, ' ');
        return;
    case AST_IfStat:
        printf("%*c:If; [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_SwitchStat:
        printf("%*c:Switch; [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_WhileStat:
        printf("%*c:While; [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_DoWhileStat:
        printf("%*c:DoWhile; [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_ForStat:
        printf("%*c:For; [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_GotoStat:
        printf("%*c:Goto %zu; [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.id,
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        return;
    case AST_ContinueStat:
        printf("%*c:Continue; [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        return;
    case AST_BreakStat:
        printf("%*c:Break; [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        return;
    case AST_ReturnStat:
        printf("%*c:Return; [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        if (ast.expr) {
            print_tree(ast.expr, depth + 2);
        }
        printf("%*c::\n", depth, ' ');
        return;
    case AST_Identifier:
        printf("%*c:Id %zu: [%p, %zu, %zu, %zu]\n", depth, ' ', ast.id,
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        return;
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
            return;
        case Char:
        case CharU8:
        case CharU16:
        case CharU32:
        case CharWide:
            printf("%*c:ConstantChar%d val: %zu: [%p, %zu, %zu, %zu]\n", depth,
                   ' ', ast.constant.type, ast.constant.c, ast.span.start,
                   ast.span.len, ast.span.row, ast.span.col);
            return;
        case Float:
        case Double:
        case LongDouble:
            printf("%*c:ConstantFloat%d val: %f: [%p, %zu, %zu, %zu]\n", depth,
                   ' ', ast.constant.type, ast.constant.d, ast.span.start,
                   ast.span.len, ast.span.row, ast.span.col);
            return;
        }
    case AST_String:
        printf("%*c:String%d id: %zu: [%p, %zu, %zu, %zu]\n", depth, ' ',
               ast.string.type, ast.string.id, ast.span.start, ast.span.len,
               ast.span.row, ast.span.col);
        return;
    case AST_Expr:
        printf("%*c:Expr: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_AssignExpr:
        printf("%*c:Assign%d: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.assign.op, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        break;
    case AST_CondExpr:
        printf("%*c:Condition: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_LogOrExpr:
        printf("%*c:LogicalOr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_LogAndExpr:
        printf("%*c:LogicalAnd: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_InclOrExpr:
        printf("%*c:InclusiveOr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_ExclOrExpr:
        printf("%*c:ExclusiveOr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_AndExpr:
        printf("%*c:And: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_EqualExpr:
        printf("%*c:Equal: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_NotEqualExpr:
        printf("%*c:NotEqual: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_LessExpr:
        printf("%*c:Less: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_GreaterExpr:
        printf("%*c:Greater: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_LessEqExpr:
        printf("%*c:LessEqual: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_GreaterEqExpr:
        printf("%*c:GreaterEqual: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_ShiftLeftExpr:
        printf("%*c:ShiftLeft: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_ShiftRightExpr:
        printf("%*c:ShiftRight: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_AddExpr:
        printf("%*c:Add: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_SubExpr:
        printf("%*c:Sub: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_MultExpr:
        printf("%*c:Mult: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_DivExpr:
        printf("%*c:Div: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_ModExpr:
        printf("%*c:Mod: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_CastExpr:
        printf("%*c:Cast: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        // TODO: This should be a type
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c::\n", depth, ' ');
        return;
    case AST_PreIncExpr:
        printf("%*c:PreInc: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_PreDecExpr:
        printf("%*c:PreDec: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_RefExpr:
        printf("%*c:RefExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_DerefExpr:
        printf("%*c:DerefExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_NegExpr:
        printf("%*c:NegExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_InvExpr:
        printf("%*c:InvExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_NotExpr:
        printf("%*c:NotExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_SizeofExpr:
        printf("%*c:SizeofExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_AlignofExpr:
        printf("%*c:AlignofExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_ArrAccessExpr:
        printf("%*c:ArrAccess: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_CallExpr:
        printf("%*c:Call: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_AccessExpr:
        printf("%*c:Access %zu: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.access.id, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        break;
    case AST_DerefAccessExpr:
        printf("%*c:DerefAccess %zu: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.access.id, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        break;
    case AST_PostIncExpr:
        printf("%*c:PostInc: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_PostDecExpr:
        printf("%*c:PostDec: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        break;
    }
    print_tree(ast.expr, depth + 2);
    printf("%*c::\n", depth, ' ');
}

void print_tree(Tree *tree, int depth) {
    for (uint64_t i = 0; i < tree->length; i++) {
        print_ast(*(Ast *)at_elem_vec((Tree *)tree, i), depth);
    }
}

void delete_ast(Ast ast) {
    switch (ast.type) {
    case AST_Invalid:
    case AST_EofInvalid:
    case AST_Eof:
    case AST_IdLabel:
    case AST_DefaultLabel:
    case AST_GotoStat:
    case AST_ContinueStat:
    case AST_BreakStat:
    case AST_Identifier:
    case AST_Constant:
    case AST_String:
    case AST_StorageSpecifier:
    case AST_FlatTypeSpecifier:
    case AST_FunctionSpecifier:
        return;
    case AST_LexList:
        if (ast.lexes) {
            delete_vec(ast.lexes);
        }
        return;
    case AST_AttributeList:
    case AST_CaseLabel:
    case AST_Labeled:
    case AST_ExprStat:
    case AST_CompStat:
    case AST_IfStat:
    case AST_SwitchStat:
    case AST_WhileStat:
    case AST_DoWhileStat:
    case AST_ForStat:
    case AST_ReturnStat:
        if (ast.expr) {
            delete_tree(ast.expr);
        }
        return;
    case AST_Attribute:
    case AST_Attributed:
    case AST_TypeSpecifier:
    case AST_Specifiers:
    case AST_InitDecl:
    case AST_StaticAssertDecl:
    case AST_Declarations:
    case AST_Expr:
    case AST_AssignExpr:
    case AST_CondExpr:
    case AST_LogOrExpr:
    case AST_LogAndExpr:
    case AST_InclOrExpr:
    case AST_ExclOrExpr:
    case AST_AndExpr:
    case AST_EqualExpr:
    case AST_NotEqualExpr:
    case AST_LessExpr:
    case AST_GreaterExpr:
    case AST_LessEqExpr:
    case AST_GreaterEqExpr:
    case AST_ShiftLeftExpr:
    case AST_ShiftRightExpr:
    case AST_AddExpr:
    case AST_SubExpr:
    case AST_MultExpr:
    case AST_DivExpr:
    case AST_ModExpr:
    case AST_CastExpr:
    case AST_PreIncExpr:
    case AST_PreDecExpr:
    case AST_RefExpr:
    case AST_DerefExpr:
    case AST_NegExpr:
    case AST_InvExpr:
    case AST_NotExpr:
    case AST_SizeofExpr:
    case AST_AlignofExpr:
    case AST_ArrAccessExpr:
    case AST_CallExpr:
    case AST_AccessExpr:
    case AST_DerefAccessExpr:
    case AST_PostIncExpr:
    case AST_PostDecExpr:
        delete_tree(ast.expr);
    }
}

void delete_tree(Tree *tree) {
    for (uint64_t i = 0; i < tree->length; i++) {
        delete_ast(*(Ast *)at_elem_vec((Tree *)tree, i));
    }
    delete_vec(tree);
}
