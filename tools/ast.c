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
    u32 level;       // NOTE(michiel): Higher level gets priority
    b32 commutative; // NOTE(michiel): A op B == B op A
    TokenKind op;
    OpAssociativity associate;
} OpPrecedence;

global OpPrecedence gOpPrecedence[NUM_TOKENS] =
{
    [TOKEN_POW] = {7, false, TOKEN_POW, Associate_RightToLeft},
    ['*']       = {6, true,  '*',       Associate_LeftToRight},
    ['/']       = {6, true,  '/',       Associate_LeftToRight},
    [TOKEN_SLL] = {5, false, TOKEN_SLL, Associate_LeftToRight},
    [TOKEN_SRA] = {5, false, TOKEN_SRA, Associate_LeftToRight},
    [TOKEN_SRL] = {5, false, TOKEN_SRL, Associate_LeftToRight},
    ['+']       = {4, true,  '+',       Associate_LeftToRight},
    ['-']       = {4, false, '-',       Associate_LeftToRight},
    ['&']       = {3, true,  '&',       Associate_LeftToRight},
    ['^']       = {2, true,  '^',       Associate_LeftToRight},
    ['|']       = {1, true,  '|',       Associate_LeftToRight},
};

internal inline OpPrecedence
get_op_precedence(TokenKind op)
{
    i_expect(op < NUM_TOKENS);
    return gOpPrecedence[op];
}

internal inline OpPrecedence
ast_get_op_precedence(AstParser *parser)
{
    i_expect(parser->current);
    return get_op_precedence(parser->current->kind);
}

internal Expr *
ast_expression_operator(AstParser *parser, u32 precedenceLevel)
{
    Expr *expr = ast_expression_unary(parser);
    
    OpPrecedence opP = ast_get_op_precedence(parser);
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
        
        opP = ast_get_op_precedence(parser);
         }
    
    return expr;
}

internal Expr *
ast_expression(AstParser *parser)
{
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
print_expr(Expr *expr)
{
    switch (expr->kind)
    {
        case Expr_Paren: { 
            fprintf(stdout, "(");
            print_expr(expr->paren.expr);
            fprintf(stdout, ")");
        } break;
        
        case Expr_Int: { fprintf(stdout, "%ld", expr->intConst); } break;
        case Expr_Id:  { fprintf(stdout, "%.*s", expr->name.size, expr->name.data); } break;
        
        case Expr_Unary:
        {
            fprintf(stdout, "[");
            switch (expr->unary.op)
            {
                case TOKEN_INC: { fprintf(stdout, "++"); } break;
                case TOKEN_DEC: { fprintf(stdout, "--"); } break;
                default:        { fprintf(stdout, "%c", expr->unary.op); } break;
            }
            print_expr(expr->unary.expr);
            fprintf(stdout, "]");
        } break;
        
        case Expr_Binary:
        {
            fprintf(stdout, "[");
            switch (expr->binary.op)
            {
                case TOKEN_POW: { fprintf(stdout, "**"); } break;
                case TOKEN_SLL: { fprintf(stdout, "<<"); } break;
                case TOKEN_SRA: { fprintf(stdout, ">>"); } break;
                case TOKEN_SRL: { fprintf(stdout, ">>>"); } break;
                default:        { fprintf(stdout, "%c", expr->binary.op); } break;
            }
            fprintf(stdout, " ");
            print_expr(expr->binary.left);
            fprintf(stdout, " ");
            print_expr(expr->binary.right);
            fprintf(stdout, "]");
        } break;
        
        INVALID_DEFAULT_CASE;
    }
}

internal void
print_ast(FileStream output, StmtList *statements)
{
    for (u32 stmtIdx = 0; stmtIdx < statements->stmtCount; ++stmtIdx)
    {
        Stmt *stmt = statements->stmts[stmtIdx];
        fprintf(output.file, "Stmt %d:\n", stmtIdx);
        if (stmt->kind == Stmt_Assign)
        {
            fprintf(output.file, "  assign: ");
            print_expr(stmt->assign.left);
            fprintf(output.file, " = ");
            print_expr(stmt->assign.right);
        }
        else
        {
            i_expect(stmt->kind == Stmt_Hint);
            fprintf(output.file, "  hint: ");
            print_expr(stmt->expr);
        }
        fprintf(output.file, "\n");
    }
}

global Map gAstSymbols_;
global Map *gAstSymbols = &gAstSymbols_;

internal void
free_expr(AstOptimizer *optimizer, Expr *expr)
{
    expr->kind = Expr_None;
    expr->nextFree = optimizer->exprFreeList;
    optimizer->exprFreeList = expr;
}

internal s64
execute_op(TokenKind op, s64 left, s64 right)
{
    s64 val = 0;
    switch ((u32)op)
    {
        case TOKEN_POW: { val = (s64)(pow((f64)left, (f64)right)); } break;
        case '*': { val = left * right; } break;
        case '/': { i_expect(right); val = left / right; } break;
        case TOKEN_SLL: { val = left << right; } break;
        case TOKEN_SRA: { val = (s64)((s64)left >> (s64)right); } break;
        case TOKEN_SRL: { val = (s64)((u64)left >> (u64)right); } break;
        case '+': { val = left + right; } break;
        case '-': { val = left - right; } break;
        case '&': { val = left & right; } break;
        case '^': { val = left ^ right; } break;
        case '|': { val = left | right; } break;
        INVALID_DEFAULT_CASE;
    }
    return val;
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
                s64 val = execute_op(expr->binary.op, left, right);
                
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
                    OpPrecedence opP = get_op_precedence(op);
                    TokenKind leftOp = left->binary.op;
                    OpPrecedence leftOpP = get_op_precedence(leftOp);
                    
                    s64 leftVal = left->binary.right->intConst;
                    s64 rightVal = expr->binary.right->intConst;
                    s64 val = 0;
                    b32 update = false;
                    
                    // NOTE(michiel): If the left operator is of lesser precedence than the
                    // current or they are the same and commutative or they are the same and
                    // it is right associative.
                    // TODO(michiel): Right associativity handling
                    b32 execute = ((leftOpP.level < opP.level) ||
                                   ((opP.op == leftOpP.op) && 
                                    (opP.commutative || 
                                     (opP.associate == Associate_RightToLeft))));
                    
                    if (execute)
                    {
                         val = execute_op(op, leftVal, rightVal);
                        update = true;
                    }
                    else if (((op == '+') || (op == '-')) && 
                             ((leftOp == '+') || (leftOp == '-')))
                    {
                        // NOTE(michiel): If we got X - 2 + 3 we can combine it to X + 1
                        // X - 2 + 3 => X + (-2 + 3) => X + (3 - 2) => X - -(3 - 2)
                        // X - (2 - 3) => X - -1
                        
                        if (((op == '+') && (leftOp == '-')) ||
                            ((op == '-') && (leftOp == '+')))
                        {
                            val = leftVal - rightVal;
                            update = true;
                        }
                        else if ((op == '-') && (leftOp == '-'))
                        {
                            val = leftVal + rightVal;
                            update = true;
                        }
                        
                        if (update && (val < 0))
                        {
                            // NOTE(michiel): Switch op and negate
                            leftOp = (leftOp == '+') ? '-' : '+';
                            val = -val;
                        }
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

internal void
insert_var_expr(AstOptimizer *optimizer, Expr *expr)
{
    switch (expr->kind)
    {
        
        case Expr_Paren:
        {
            insert_var_expr(optimizer, expr->paren.expr);
        } break;
        
        case Expr_Int:
        {
            // NOTE(michiel): Do nothing
        } break;
        
        case Expr_Id:
        {
            if (!strings_are_equal(expr->name, create_string("IO")))
            {
                Expr *varExpr = map_get(gAstSymbols, expr->name.data);
                if (varExpr)
                {
                    expr->kind = Expr_Paren;
                    expr->paren.expr = varExpr;
                }
            }
        } break;
        
        case Expr_Unary:
        {
            insert_var_expr(optimizer, expr->unary.expr);
        } break;
        
        case Expr_Binary:
        {
            insert_var_expr(optimizer, expr->binary.left);
            insert_var_expr(optimizer, expr->binary.right);
        } break;
        
        INVALID_DEFAULT_CASE;
    }
}

internal void
ast_optimize(AstOptimizer *optimizer)
{

#if 0    
    for (u32 stmtIdx = 0; stmtIdx < optimizer->statements.stmtCount; ++stmtIdx)
    {
        Stmt *stmt = optimizer->statements.stmts[stmtIdx];
        if (stmt->kind == Stmt_Assign)
        {
            i_expect(stmt->assign.left->kind == Expr_Id);
            if (!strings_are_equal(stmt->assign.left->name, create_string("IO")))
            {
                map_put(gAstSymbols, stmt->assign.left->name.data, stmt->assign.right);
            }
            insert_var_expr(optimizer, stmt->assign.right);
            }
    }
    #endif

    for (u32 stmtIdx = 0; stmtIdx < optimizer->statements.stmtCount; ++stmtIdx)
    {
        Stmt *stmt = optimizer->statements.stmts[stmtIdx];
        if (stmt->kind == Stmt_Assign)
        {
            combine_const(optimizer, stmt->assign.left);
            combine_const(optimizer, stmt->assign.right);
            }
        else
        {
            combine_const(optimizer, stmt->expr);
        }
    }
}

// TODO(michiel): print immediate repr
