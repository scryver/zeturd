global Arena astArena;

#define ast_alloc_array(type, count) (type *)ast_alloc(sizeof(type) * count)
#define ast_alloc_struct(type)       (type *)ast_alloc(sizeof(type))
internal void *
ast_alloc(u64 size)
{
    i_expect(size);
    void *ptr = arena_allocate(&astArena, size);
    memset(ptr, 0, size);
    return ptr;
}

internal Expr *
ast_alloc_expr(AstOptimizer *optimizer)
{
    Expr *result = 0;
    if (optimizer)
    {
         result = optimizer->exprFreeList;
        if (result)
        {
            optimizer->exprFreeList = result->nextFree;
        }
    }
    
    if (!result)
    {
        result = ast_alloc_struct(Expr);
    }
    return result;
}

//
// NOTE(michiel): Init functions
//
internal Expr *
create_expr(ExprKind kind)
{
    Expr *result = ast_alloc_expr(0);
    result->kind = kind;
    return result;
}

internal Expr *
create_paren_expr(Expr *expr)
{
    Expr *result = create_expr(Expr_Paren);
    result->paren.expr = expr;
    return result;
}

internal Expr *
create_int_expr(s64 value)
{
    Expr *result = create_expr(Expr_Int);
    result->intConst = value;
    return result;
}

internal Expr *
create_id_expr(String id)
{
    Expr *result = create_expr(Expr_Id);
    result->name = str_internalize(id);
    return result;
}

internal Expr *
create_unary_expr(TokenKind op, Expr *expr)
{
    Expr *result = create_expr(Expr_Unary);
    result->unary.op = op;
    result->unary.expr = expr;
    return result;
}

internal Expr *
create_binary_expr(TokenKind op, Expr *left, Expr *right)
{
    Expr *result = create_expr(Expr_Binary);
    result->binary.op = op;
    result->binary.left = left;
    result->binary.right = right;
    return result;
}

internal Stmt *
create_stmt(StmtKind kind)
{
    Stmt *result = ast_alloc_struct(Stmt);
    result->kind = kind;
    return result;
}

internal Stmt *
create_assign_stmt(TokenKind op, Expr *left, Expr *right)
{
    Stmt *result = create_stmt(Stmt_Assign);
    result->assign.op = op;
    result->assign.left = left;
    result->assign.right = right;
    return result;
}

internal Stmt *
create_hint_stmt(Expr *expr)
{
    Stmt *result = create_stmt(Stmt_Hint);
    result->expr = expr;
    return result;
}

//
// NOTE(michiel): Parsing of tokens
//

internal void
ast_next_token(AstParser *parser)
{
    parser->current = parser->current->nextToken;
}

internal b32
is_token(AstParser *parser, TokenKind kind)
{
    return parser->current->kind == kind;
}

internal b32
match_token(AstParser *parser, TokenKind kind)
{
    b32 result = false;
    if (is_token(parser, kind))
    {
        result = true;
    ast_next_token(parser);
    }
    return result;
}

internal b32
expect_token(AstParser *parser, TokenKind kind)
{
    b32 result = false;
    if (is_token(parser, kind))
    {
        result = true;
        ast_next_token(parser);
    }
    else
    {
        fprintf(stderr, "Expected token ");
        print_token_kind((FileStream){.file=stderr}, kind);
        fprintf(stderr, ", got ");
        print_token(((FileStream){.file=stderr}), parser->current);
        fprintf(stderr, "\n");
        INVALID_CODE_PATH;
    }
    return result;
}

internal Expr *ast_expression(AstParser *parser);

internal Expr *
ast_expression_operand(AstParser *parser)
{
    Expr *result = 0;
    if (is_token(parser, TOKEN_NUMBER))
    {
        u64 val = string_to_number(parser->current->value);
        ast_next_token(parser);
        result = create_int_expr(val);
    }
    else if (is_token(parser, TOKEN_ID))
    {
        String name = parser->current->value;
        ast_next_token(parser);
        result = create_id_expr(name);
    }
    else if (is_token(parser, '('))
    {
        expect_token(parser, '(');
        Expr *expr = ast_expression(parser);
        expect_token(parser, ')');
        result = create_paren_expr(expr);
    }
    else
    {
        // NOTE(michiel): ERROR
        fprintf(stderr, "Expected INT, ID or (, got ");
        print_token(((FileStream){.file=stderr}), parser->current);
        fprintf(stderr, "\n");
        INVALID_CODE_PATH;
    }
    
    return result;
}

internal b32
is_unary_op(AstParser *parser)
{
    b32 result = false;
    if (is_token(parser, '+') ||
        is_token(parser, '-') ||
        is_token(parser, '~') ||
        is_token(parser, '!') ||
        is_token(parser, TOKEN_INC) ||
        is_token(parser, TOKEN_DEC))
    {
        result = true;
    }
    return result;
}

internal Expr *
ast_expression_unary(AstParser *parser)
{
    Expr *result = 0;
    if (is_unary_op(parser))
    {
        TokenKind op = parser->current->kind;
        ast_next_token(parser);
        result = create_unary_expr(op, ast_expression_unary(parser));
    }
    else
    {
        result = ast_expression_operand(parser);
    }
    return result;
}

typedef enum OpAssociativity
{
    Associate_None,
    Associate_LeftToRight,
    Associate_RightToLeft,
} OpAssociativity;
typedef struct OpPrecedence
{
    u32 level;
    TokenKind op;
    OpAssociativity associate;
} OpPrecedence;

global OpPrecedence gOpPrecedence[NUM_TOKENS] =
{
    ['*']       = {6, '*',       Associate_LeftToRight},
    ['/']       = {6, '/',       Associate_LeftToRight},
    [TOKEN_SLL] = {5, TOKEN_SLL, Associate_LeftToRight},
    [TOKEN_SRA] = {5, TOKEN_SRA, Associate_LeftToRight},
    [TOKEN_SRL] = {5, TOKEN_SRL, Associate_LeftToRight},
    ['+']       = {4, '+',       Associate_LeftToRight},
    ['-']       = {4, '-',       Associate_LeftToRight},
    ['&']       = {3, '&',       Associate_LeftToRight},
    ['^']       = {2, '^',       Associate_LeftToRight},
    ['|']       = {1, '|',       Associate_LeftToRight},
};

internal inline OpPrecedence
get_op_precedence(AstParser *parser)
{
    i_expect(parser->current);
    i_expect(parser->current->kind < NUM_TOKENS);
    return gOpPrecedence[parser->current->kind];
}

internal Expr *
ast_expression_operator(AstParser *parser, u32 precedenceLevel)
{
    Expr *expr = ast_expression_unary(parser);
    
    OpPrecedence opP = get_op_precedence(parser);
    while ((opP.associate != Associate_None) &&
           (opP.level >= precedenceLevel))
        {
            ast_next_token(parser);
            if (opP.associate == Associate_LeftToRight)
        {
            expr = create_binary_expr(opP.op, expr,
                                      ast_expression_operator(parser, opP.level + 1));
            }
            else
            {
                i_expect(opP.associate == Associate_RightToLeft);
            expr = create_binary_expr(opP.op, expr, 
                                      ast_expression_operator(parser, opP.level));
            }
        
        opP = get_op_precedence(parser);
         }
    
    return expr;
}

internal b32
is_mul_op(AstParser *parser)
{
    b32 result = false;
    if (is_token(parser, '*') ||
        is_token(parser, '/') ||
        is_token(parser, '&') ||
        is_token(parser, TOKEN_SLL) ||
        is_token(parser, TOKEN_SRA) ||
        is_token(parser, TOKEN_SRL))
    {
    result = true;
    }
    return result;
}

internal Expr *
ast_expression_mul(AstParser *parser)
{
    Expr *expr = ast_expression_unary(parser);
    while (is_mul_op(parser))
    {
        TokenKind op = parser->current->kind;
        ast_next_token(parser);
        expr = create_binary_expr(op, expr, ast_expression_unary(parser));
    }
    return expr;
}

internal b32
is_add_op(AstParser *parser)
{
    b32 result = false;
    if (is_token(parser, '+') ||
        is_token(parser, '-') ||
        is_token(parser, '^') ||
        is_token(parser, '|'))
    {
    result = true;
    }
    return result;
}

internal Expr *
ast_expression_add(AstParser *parser)
{
    Expr *expr = ast_expression_mul(parser);
    while (is_add_op(parser))
    {
        TokenKind op = parser->current->kind;
        ast_next_token(parser);
        expr = create_binary_expr(op, expr, ast_expression_mul(parser));
    }
    return expr;
}

internal Expr *
ast_expression(AstParser *parser)
{
    //return ast_expression_add(parser);
    return ast_expression_operator(parser, 0);
}

internal Stmt *
ast_statement(AstParser *parser)
{
    Expr *expr = ast_expression(parser);
    Stmt *result = 0;
    if (is_token(parser, '='))
    {
        TokenKind op = parser->current->kind;
        ast_next_token(parser);
        result = create_assign_stmt(op, expr, ast_expression(parser));
    }
    else
    {
        result = create_hint_stmt(expr);
    }
    
    return result;
}

internal b32
is_end_statement(AstParser *parser)
{
    b32 result = false;
    if (is_token(parser, TOKEN_EOF) ||
        is_token(parser, '\n') ||
        is_token(parser, ';'))
    {
        result = true;
    }
    return result;
}

internal StmtList *
ast_from_tokens(Token *tokens)
{
    AstParser parser = {0};
    parser.tokens = tokens;
    parser.current = tokens;
    
    StmtList *result = ast_alloc_struct(StmtList);
    
    while (parser.current)
    {
        Stmt *stmt = ast_statement(&parser);
        buf_push(result->stmts, stmt);
        ++result->stmtCount;
        do
        {
            if (!is_end_statement(&parser))
            {
                fprintf(stderr, "Statement not closed by newline or semi-colon!\nStuck at: ");
                print_token((FileStream){.file=stderr}, parser.current);
                fprintf(stderr, "\n");
            }
            i_expect(is_end_statement(&parser));
            ast_next_token(&parser);
        } while (parser.current && is_end_statement(&parser));
    }
    
    return result;
}

internal void
free_expr(AstOptimizer *optimizer, Expr *expr)
{
    expr->kind = Expr_None;
    expr->nextFree = optimizer->exprFreeList;
    optimizer->exprFreeList = expr;
}

internal void
combine_const(AstOptimizer *optimizer, Expr *expr)
{
    // NOTE(michiel): This __DOESN'T__ free the expression that have been combined
    // TODO(michiel): Maybe freelist them?
    switch (expr->kind)
    {
        case Expr_Paren:
        {
            combine_const(optimizer, expr->paren.expr);
            if (expr->paren.expr->kind == Expr_Int)
            {
                Expr *parenExpr = expr->paren.expr;
                s64 val = parenExpr->intConst;
                expr->kind = Expr_Int;
                expr->intConst = val;
                free_expr(optimizer, parenExpr);
            }
        } break;
        
        case Expr_Int:
        {
            // NOTE(michiel): Do nothing
        } break;
        
        case Expr_Id:
        {
            // NOTE(michiel): Do nothing
        } break;
        
        case Expr_Unary:
        {
            combine_const(optimizer, expr->unary.expr);
            Expr *unaryExpr = expr->unary.expr;
            if (unaryExpr->kind == Expr_Int)
            {
                s64 val = unaryExpr->intConst;
                switch ((u32)expr->unary.op)
                {
                    case '+': { } break;
                    case '-': { val = -val; } break;
                    case '~': { val = ~val; } break;
                    case '!': { val = !val; } break;
                    case TOKEN_INC: { val = val + 1; } break;
                    case TOKEN_DEC: { val = val - 1; } break;
                    INVALID_DEFAULT_CASE;
                }
                expr->kind = Expr_Int;
                expr->intConst = val;
                free_expr(optimizer, unaryExpr);
            }
        } break;
        
        case Expr_Binary:
        {
            combine_const(optimizer, expr->binary.left);
            combine_const(optimizer, expr->binary.right);
            if ((expr->binary.left->kind == Expr_Int) &&
                (expr->binary.right->kind == Expr_Int))
            {
                s64 left = expr->binary.left->intConst;
                s64 right = expr->binary.right->intConst;
                s64 val = 0;
                switch ((u32)expr->binary.op)
                {
                    case '*': { val = left * right; } break;
                    case '/': { i_expect(right); val = left / right; } break;
                    case '&': { val = left & right; } break;
                    case TOKEN_SLL: { val = left << right; } break;
                    case TOKEN_SRA: { val = (s64)((s64)left >> (s64)right); } break;
                    case TOKEN_SRL: { val = (s64)((u64)left >> (u64)right); } break;
                    case '+': { val = left + right; } break;
                    case '-': { val = left - right; } break;
                    case '^': { val = left ^ right; } break;
                    case '|': { val = left | right; } break;
                    INVALID_DEFAULT_CASE;
                }
                
                free_expr(optimizer, expr->binary.left);
                free_expr(optimizer, expr->binary.right);
                expr->kind = Expr_Int;
                expr->intConst = val;
            }
            else if (expr->binary.right->kind == Expr_Int)
            {
                Expr *left = expr->binary.left;
                
                if ((expr->binary.left->kind == Expr_Binary) &&
                    (expr->binary.left->binary.right->kind == Expr_Int))
                {
                    TokenKind op = expr->binary.op;
                    TokenKind leftOp = left->binary.op;
                    
                    s64 leftVal = left->binary.right->intConst;
                    s64 rightval = expr->binary.right->intConst;
                    s64 val = 0;
                    b32 update = false;
                    
                    switch ((u32)op)
                    {
                        case '*': {
                            if ((leftOp == '*') ||
                                (leftOp == '+') ||
                                (leftOp == '-') ||
                                (leftOp == '^') ||
                                (leftOp == '|'))
                            {
                                val = leftVal * rightval;
                                update = true; 
                            }
                        } break;
                        
                        case '/': {
                            if ((leftOp == '*') ||
                                (leftOp == '+') ||
                                (leftOp == '-') ||
                                (leftOp == '^') ||
                                (leftOp == '|'))
                            {
                                i_expect(rightval);
                                val = leftVal / rightval;
                                update = true;
                            }
                        } break;
                        
                        case '&': {
                            if ((leftOp == '&') ||
                                (leftOp == '+') ||
                                (leftOp == '-') ||
                                (leftOp == '^') ||
                                (leftOp == '|'))
                            {
                                val = leftVal & rightval;
                                update = true; 
                            }
                        } break;
                        
                        case TOKEN_SLL: {
                            if ((leftOp == '+') ||
                                (leftOp == '-') ||
                                (leftOp == '^') ||
                                (leftOp == '|'))
                            {
                                val = leftVal << rightval;
                                update = true; 
                            }
                        } break;
                        
                        case TOKEN_SRA: {
                            if ((leftOp == '+') ||
                                (leftOp == '-') ||
                                (leftOp == '^') ||
                                (leftOp == '|'))
                            {
                                val = (s64)((s64)leftVal >> (s64)rightval);
                                update = true; 
                            }
                        } break;
                        
                        case TOKEN_SRL: {
                            if ((leftOp == '+') ||
                                (leftOp == '-') ||
                                (leftOp == '^') ||
                                (leftOp == '|'))
                            {
                                val = (s64)((u64)leftVal >> (u64)rightval);
                                update = true; 
                            }
                        } break;
                        
                        case '+': {
                            if ((leftOp == '+') ||
                                (leftOp == '^') ||
                                (leftOp == '|'))
                            {
                                val = leftVal + rightval;
                                update = true;
                            }
                            else if (leftOp == '-')
                            {
                                val = leftVal - rightval;
                                update = true;
                            }
                        } break;
                        
                        case '-': { 
                            if ((leftOp == '+') ||
                                (leftOp == '^') ||
                                (leftOp == '|'))
                            {
                                val = leftVal - rightval;
                                update = true;
                            }
                            else if (leftOp == '-')
                            {
                                val = leftVal + rightval;
                                update = true;
                            }
                        } break;
                        
                        case '^': {
                            if ((leftOp == '^') ||
                                (leftOp == '|'))
                            {
                                val = leftVal ^ rightval;
                                update = true; 
                            }
                        } break;
                        
                        case '|': {
                            if (leftOp == '|')
                            {
                                val = leftVal | rightval;
                                update = true; 
                            }
                        } break;
                        
                        INVALID_DEFAULT_CASE;
                    }
                    
                    if (update)
                    {
                        expr->binary.op = leftOp;
                        expr->binary.left = left->binary.left;
                        expr->binary.right->intConst = val;
                        free_expr(optimizer, left->binary.right);
                    free_expr(optimizer, left);
                        }
                }
            }
        } break;
        
        INVALID_DEFAULT_CASE;
    }
}

