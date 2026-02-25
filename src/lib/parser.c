#include "parser.h"
#include "lexer.h"
#include "pp.h"
#include "vec.h"
#include <stdio.h>

Lex parse_next(Parser *parser) {
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
        return;
    }

    size_t idx = *(size_t *)peek_elem_vec(parser->idx_stack);
    pop_elem_vec(parser->idx_stack);
    parser->ctx->length = parser->idx;
    parser->idx = idx;
}

void erase_ctx(Parser *parser) {
    if (!parser->idx_stack->length) {
        return reset_ctx(parser);
    }

    size_t idx = *(size_t *)peek_elem_vec(parser->idx_stack);
    for (size_t i = 0; i < parser->idx - idx; i++) {
        pop_elem_vec(parser->ctx);
    }
    pop_elem_vec(parser->idx_stack);
    parser->idx = idx;
}

Ast primary_expression(Parser *parser) {
    Lex lex = parse_next(parser);
    switch (lex.type) {
    case LEX_Eof:
        return (Ast){
            .type = AST_Eof,
        };
    case LEX_Identifier:
        return (Ast){
            .type = AST_Identifier,
            .span = lex.span,
            .id = lex.id,
        };
    case LEX_String:
        return (Ast){
            .type = AST_String,
            .span = lex.span,
            .string = {.type = ASCII, .id = lex.id},
        };
    case LEX_StringU8:
        return (Ast){
            .type = AST_String,
            .span = lex.span,
            .string = {.type = U8, .id = lex.id},
        };
    case LEX_StringU16:
        return (Ast){
            .type = AST_String,
            .span = lex.span,
            .string = {.type = U16, .id = lex.id},
        };
    case LEX_StringU32:
        return (Ast){
            .type = AST_String,
            .span = lex.span,
            .string = {.type = U32, .id = lex.id},
        };
    case LEX_StringWide:
        return (Ast){
            .type = AST_String,
            .span = lex.span,
            .string = {.type = Wide, .id = lex.id},
        };
    case LEX_ConstantUnsignedLongLong:
        return (Ast){
            .type = AST_Constant,
            .span = lex.span,
            .constant = {.type = UnsignedLongLong, .c = lex.constant},
        };
    case LEX_ConstantUnsignedLong:
        return (Ast){
            .type = AST_Constant,
            .span = lex.span,
            .constant = {.type = UnsignedLong, .c = lex.constant},
        };
    case LEX_ConstantUnsignedBitPrecise:
        return (Ast){
            .type = AST_Constant,
            .span = lex.span,
            .constant = {.type = UnsignedBitPrecise, .c = lex.constant}};
    case LEX_ConstantUnsigned:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = Unsigned, .c = lex.constant}};
    case LEX_ConstantLongLong:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = LongLong, .c = lex.constant}};
    case LEX_ConstantLong:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = Long, .c = lex.constant}};
    case LEX_ConstantBitPrecise:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = BitPrecise, .c = lex.constant}};
    case LEX_Constant:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = Int, .c = lex.constant}};
    case LEX_ConstantFloat:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = Float, .c = lex.constant}};
    case LEX_ConstantDouble:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = Double, .c = lex.constant}};
    case LEX_ConstantLongDouble:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = LongDouble, .c = lex.constant}};
    case LEX_ConstantDecimal32:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = Decimal32, .c = lex.constant}};
    case LEX_ConstantDecimal64:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = Decimal64, .c = lex.constant}};
    case LEX_ConstantDecimal128:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = Decimal128, .c = lex.constant}};
    case LEX_ConstantChar:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = Char, .c = lex.constant}};
    case LEX_ConstantCharU8:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = CharU8, .c = lex.constant}};
    case LEX_ConstantCharU16:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = CharU16, .c = lex.constant}};
    case LEX_ConstantCharU32:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = CharU32, .c = lex.constant}};
    case LEX_ConstantCharWide:
        return (Ast){.type = AST_Constant,
                     .span = lex.span,
                     .constant = {.type = CharWide, .c = lex.constant}};
    default:
        return (Ast){.type = AST_Invalid,
                     .span = lex.span,
                     .invalid = {.type = BadPrimaryExpression}};
    }
}

Ast conditional_expression(Parser *parser) {
    return primary_expression(parser);
}

Ast unary_expression(Parser *parser) { return primary_expression(parser); }

Ast asssignment_expression(Parser *parser) {
    save_ctx(parser);
    Ast ast = unary_expression(parser);
    if (ast.type == AST_Eof) {
        return ast;
    }

    enum assign_op op;
    Lex lex = parse_next(parser);
    switch (lex.type) {
    case LEX_Equal:
        op = Assign;
        break;
    case LEX_StarEqual:
        op = AssignMul;
        break;
    case LEX_SlashEqual:
        op = AssignDiv;
        break;
    case LEX_PercentEqual:
        op = AssignMod;
        break;
    case LEX_PlusEqual:
        op = AssignAdd;
        break;
    case LEX_MinusEqual:
        op = AssignSub;
        break;
    case LEX_LeftLeftEqual:
        op = AssignLShift;
        break;
    case LEX_RightRightEqual:
        op = AssignRShift;
        break;
    case LEX_EtEqual:
        op = AssignAnd;
        break;
    case LEX_CaretEqual:
        op = AssignXor;
        break;
    case LEX_PipeEqual:
        op = AssignOr;
        break;
    default:
        return_ctx(parser);
        return conditional_expression(parser);
    }
    Tree *tree = create_tree(2);
    push_elem_vec(&tree, &ast);
    ast = asssignment_expression(parser);
    push_elem_vec(&tree, &ast);

    erase_ctx(parser);
    return (Ast){.type = AST_AssignExpr,
                 .span = lex.span,
                 .assign = {.assigns = tree, .op = op}};
}

Ast expression(Parser *parser) {
    Ast ast = asssignment_expression(parser);
    save_ctx(parser);
    Lex lex = parse_next(parser);
    switch (lex.type) {
    case LEX_Comma: {
        erase_ctx(parser);
        Tree *assignments = create_tree(2);
        push_elem_vec(&assignments, &ast);
        while (1) {
            Ast ast = asssignment_expression(parser);
            push_elem_vec(&assignments, &ast);
            save_ctx(parser);
            Lex lex = parse_next(parser);
            if (lex.type != LEX_Comma) {
                return_ctx(parser);
                return (Ast){
                    .type = AST_Expr, .span = lex.span, .expr = assignments};
            }
            erase_ctx(parser);
        }
    }
    default:
        return_ctx(parser);
        return ast;
    }
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
    printf("(PARSER: idxs:\n");
    for (size_t i = 0; i < parser->idx_stack->length; i++) {
        printf(" %zu\n", *(size_t *)at_elem_vec(parser->idx_stack, i));
    }
    printf("lexes:\n");
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

void print_tree(Tree *tree) {
    for (uint64_t i = 0; i < tree->length; i++) {
        Ast ast = *(Ast *)at_elem_vec((Tree *)tree, i);
        switch (ast.type) {
        case AST_Invalid:
            printf(":Ast Error %d: [%p, %zu, %zu, %zu]\n", ast.invalid.type,
                   ast.span.start, ast.span.len, ast.span.row, ast.span.col);
            break;
        case AST_Eof:
            printf(":Ast Eof: [%p, %zu, %zu, %zu]\n", ast.span.start,
                   ast.span.len, ast.span.row, ast.span.col);
            break;
        case AST_Identifier:
            printf(":Id %zu: [%p, %zu, %zu, %zu]\n", ast.id, ast.span.start,
                   ast.span.len, ast.span.row, ast.span.col);
            break;
        case AST_Constant:
            printf(":Constant%d val: %zu: [%p, %zu, %zu, %zu]\n",
                   ast.constant.type, ast.constant.c, ast.span.start,
                   ast.span.len, ast.span.row, ast.span.col);
            break;
        case AST_String:
            printf(":String%d id: %zu: [%p, %zu, %zu, %zu]\n", ast.string.type,
                   ast.string.id, ast.span.start, ast.span.len, ast.span.row,
                   ast.span.col);
            break;
        case AST_Expr:
            printf(":Expr: [%p, %zu, %zu, %zu] ::\n", ast.span.start,
                   ast.span.len, ast.span.row, ast.span.col);
            print_tree(ast.expr);
            printf("::\n");
            break;
        case AST_AssignExpr:
            printf(":AssignExpr %d: [%p, %zu, %zu, %zu] ::\n", ast.assign.op,
                   ast.span.start, ast.span.len, ast.span.row, ast.span.col);
            print_tree(ast.assign.assigns);
            printf("::\n");
            break;
        }
    }
}

// TODO: Delete Subtrees
void delete_tree(Tree *tree) { return delete_vec(tree); }
