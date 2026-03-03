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

void erase_ctx(Parser *parser) {
    if (!parser->idx_stack->length) {
        return reset_ctx(parser);
    }

    size_t idx = *(size_t *)peek_elem_vec(parser->idx_stack);
    delete_slice_vec(parser->ctx, idx, parser->idx);
    pop_elem_vec(parser->idx_stack);
    parser->idx = idx;
}

void ignore_ctx(Parser *parser) {
    if (parser->idx_stack->length) {
        pop_elem_vec(parser->idx_stack);
    }
}

Lex erase_next(Parser *parser) {
    save_ctx(parser);
    Lex lex = next(parser);
    erase_ctx(parser);
    return lex;
}

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

Ast unary_expression(Parser *parser) { return primary_expression(parser); }
Ast rel_expression(Parser *parser) { return primary_expression(parser); }

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
        printf("%*c:AssignExpr%d: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.assign.op, ast.span.start, ast.span.len, ast.span.row,
               ast.span.col);
        print_tree(ast.assign.assigns, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_CondExpr:
        printf("%*c:CondExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_LogOrExpr:
        printf("%*c:LogOrExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_LogAndExpr:
        printf("%*c:LogAndExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_InclOrExpr:
        printf("%*c:InclOrExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_ExclOrExpr:
        printf("%*c:ExclOrExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_AndExpr:
        printf("%*c:AndExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_tree(ast.expr, depth + 2);
        printf("%*c::\n", depth, ' ');
        break;
    case AST_EqualExpr:
        printf("%*c:EqualExpr\n", depth, ' ');
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
        printf("%*c: [%p, %zu, %zu, %zu] ::\n", depth, ' ', ast.span.start,
               ast.span.len, ast.span.row, ast.span.col);
        break;
    case AST_NotEqualExpr:
        printf("%*c:NotEqualExpr: [%p, %zu, %zu, %zu] ::\n", depth, ' ',
               ast.span.start, ast.span.len, ast.span.row, ast.span.col);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 0), depth + 2);
        print_ast(*(Ast *)at_elem_vec(ast.expr, 1), depth + 2);
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
