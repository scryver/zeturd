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

internal Stmt *
ast_alloc_stmt(AstOptimizer *optimizer)
{
     Stmt *result = 0;
    if (optimizer)
    {
        result = optimizer->stmtFreeList;
        if (result)
        {
            optimizer->stmtFreeList = result->nextFree;
        }
    }
    
    if (!result)
    {
        result = ast_alloc_struct(Stmt);
    }
    return result;
}

//
// NOTE(michiel): Init functions
//
internal Expr *
create_expr(SourcePos origin, ExprKind kind)
{
    Expr *result = ast_alloc_expr(0);
    result->origin = origin;
    result->kind = kind;
    return result;
}

internal Expr *
create_paren_expr(SourcePos origin, Expr *expr)
{
    Expr *result = create_expr(origin, Expr_Paren);
    result->paren.expr = expr;
    return result;
}

internal Expr *
create_int_expr(SourcePos origin, s64 value)
{
    Expr *result = create_expr(origin, Expr_Int);
    result->intConst = value;
    return result;
}

internal Expr *
create_id_expr(SourcePos origin, String id)
{
    Expr *result = create_expr(origin, Expr_Id);
    result->name = str_internalize(id);
    return result;
}

internal Expr *
create_unary_expr(SourcePos origin, TokenKind op, Expr *expr)
{
    Expr *result = create_expr(origin, Expr_Unary);
    result->unary.op = op;
    result->unary.expr = expr;
    return result;
}

internal Expr *
create_binary_expr(SourcePos origin, TokenKind op, Expr *left, Expr *right)
{
    Expr *result = create_expr(origin, Expr_Binary);
    result->binary.op = op;
    result->binary.left = left;
    result->binary.right = right;
    return result;
}

internal Stmt *
create_stmt(SourcePos origin, StmtKind kind)
{
    Stmt *result = ast_alloc_stmt(0);
    result->origin = origin;
    result->kind = kind;
    return result;
}

internal Stmt *
create_assign_stmt(SourcePos origin, TokenKind op, Expr *left, Expr *right)
{
    Stmt *result = create_stmt(origin, Stmt_Assign);
    result->assign.op = op;
    result->assign.left = left;
    result->assign.right = right;
    return result;
}

internal Stmt *
create_hint_stmt(SourcePos origin, Expr *expr)
{
    Stmt *result = create_stmt(origin, Stmt_Hint);
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
    SourcePos origin = parser->current->origin;
    if (is_token(parser, TOKEN_LINE_COMMENT))
    {
        // NOTE(michiel): Do nothing
        ast_next_token(parser);
    } 
    else if (is_token(parser, TOKEN_NUMBER))
    {
        u64 val = string_to_number(parser->current->value);
        ast_next_token(parser);
        result = create_int_expr(origin, val);
    }
    else if (is_token(parser, TOKEN_ID))
    {
        String name = parser->current->value;
        ast_next_token(parser);
        result = create_id_expr(origin, name);
    }
    else if (is_token(parser, '('))
    {
        expect_token(parser, '(');
        Expr *expr = ast_expression(parser);
        expect_token(parser, ')');
        result = create_paren_expr(origin, expr);
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
        SourcePos origin = parser->current->origin;
        TokenKind op = parser->current->kind;
        ast_next_token(parser);
        result = create_unary_expr(origin, op, ast_expression_unary(parser));
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
    
    if (expr)
    {
    OpPrecedence opP = ast_get_op_precedence(parser);
    while ((opP.associate != Associate_None) &&
           (opP.level >= precedenceLevel))
        {
        SourcePos origin = parser->current->origin;
            ast_next_token(parser);
            if (opP.associate == Associate_LeftToRight)
        {
            expr = create_binary_expr(origin, opP.op, expr,
                                      ast_expression_operator(parser, opP.level + 1));
            }
            else
            {
                i_expect(opP.associate == Associate_RightToLeft);
            expr = create_binary_expr(origin, opP.op, expr, 
                                      ast_expression_operator(parser, opP.level));
            }
        
        opP = ast_get_op_precedence(parser);
         }
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
    SourcePos origin = parser->current->origin;
    Expr *expr = ast_expression(parser);
    Stmt *result = 0;
    if (expr)
    {
    if (is_token(parser, '='))
    {
        TokenKind op = parser->current->kind;
        ast_next_token(parser);
        result = create_assign_stmt(origin, op, expr, ast_expression(parser));
    }
    else
    {
        result = create_hint_stmt(origin, expr);
    }
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
        if (stmt)
        {
        buf_push(result->stmts, stmt);
        ++result->stmtCount;
        }
        
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

internal void
free_expr(AstOptimizer *optimizer, Expr *expr)
{
    expr->kind = Expr_None;
    expr->nextFree = optimizer->exprFreeList;
    optimizer->exprFreeList = expr;
}

internal void
free_all_expr(AstOptimizer *optimizer, Expr *expr)
{
    switch (expr->kind)
    {
        case Expr_Paren:
        {
            free_all_expr(optimizer, expr->paren.expr);
        } break;
        
        case Expr_Int:
        case Expr_Id:
        {
            // NOTE(michiel): Do nothing
        } break;
        
        case Expr_Unary:
        {
            free_all_expr(optimizer, expr->unary.expr);
        } break;
        
        case Expr_Binary:
        {
            free_all_expr(optimizer, expr->binary.left);
            free_all_expr(optimizer, expr->binary.right);
        } break;
        
        INVALID_DEFAULT_CASE;
    }
    free_expr(optimizer, expr);
}

internal void
free_stmt(AstOptimizer *optimizer, Stmt *stmt)
{
    stmt->kind = Stmt_None;
    stmt->nextFree = optimizer->stmtFreeList;
    optimizer->stmtFreeList = stmt;
}

global Map gAstSymbols_;
global Map *gAstSymbols = &gAstSymbols_;

global Map gAstSymbolExpr_;
global Map *gAstSymbolExpr = &gAstSymbolExpr_;

global String gKeyWordNames[] = 
{
    {6, (u8 *)"SYNCED"},
    {2, (u8 *)"IO"},
};

internal b32
is_key_word(String var)
{
    b32 result = false;
    
    for (u32 keyIdx = 0; keyIdx < array_count(gKeyWordNames); ++keyIdx)
    {
        if (strings_are_equal(var, gKeyWordNames[keyIdx]))
        {
            result = true;
            break;
        }
    }
    
    return result;
}

internal inline String
get_assign_name(Expr *expr)
{
    i_expect(expr->kind == Expr_Id);
    String var = expr->name;
    String result = var;
    if (!is_key_word(var))
    {
        u64 id = map_get_u64(gAstSymbols, var.data);
        ++id;
        map_put_u64(gAstSymbols, var.data, id);
        result = create_string_fmt("%.*s%d", var.size, var.data, id);
    }
    return result;
}

internal inline String
get_var_name(Expr *expr)
{
    i_expect(expr->kind == Expr_Id);
    String var = expr->name;
    String result = var;
    if (!is_key_word(var))
    {
        u64 id = map_get_u64(gAstSymbols, var.data);
        if (id)
        {
            result = create_string_fmt("%.*s%d", var.size, var.data, id);
        }
        else
        {
            fprintf(stderr, "%.*s:%d:%d: Variable %.*s has not been assigned yet!\n",
                    expr->origin.filename.size, expr->origin.filename.data,
                    expr->origin.lineNumber, expr->origin.colNumber, 
                    var.size, var.data);
            INVALID_CODE_PATH;
        }
    }
    return result;
}

internal void
assign_var_names(AstOptimizer *optimizer, Expr *expr, b32 isAssign)
{
    switch (expr->kind)
    {
        
        case Expr_Paren:
        {
            assign_var_names(optimizer, expr->paren.expr, isAssign);
        } break;
        
        case Expr_Int:
        {
            // NOTE(michiel): Do nothing
        } break;
        
        case Expr_Id:
        {
            String varName;
            if (isAssign)
            {
                varName = get_assign_name(expr);
                }
            else
            {
             varName = get_var_name(expr);
            }
            expr->name = varName;
        } break;
        
        case Expr_Unary:
        {
            assign_var_names(optimizer, expr->unary.expr, isAssign);
        } break;
        
        case Expr_Binary:
        {
            assign_var_names(optimizer, expr->binary.left, isAssign);
            assign_var_names(optimizer, expr->binary.right, isAssign);
        } break;
        
        INVALID_DEFAULT_CASE;
    }
}

internal b32
collapse_parenthesis(AstOptimizer *optimizer, Expr *expr)
{
    b32 result = false; // NOTE(michiel): Indicates changes have been made
    switch (expr->kind)
    {
        case Expr_Paren:
        {
            if (expr->paren.expr->kind == Expr_Paren)
            {
                Expr *parenExpr = expr->paren.expr;
                expr->paren.expr = parenExpr->paren.expr;
                free_expr(optimizer, parenExpr);
                result = true;
            }
            result |= collapse_parenthesis(optimizer, expr->paren.expr);
        } break;
        
        case Expr_Int:
        case Expr_Id:
        {
            // NOTE(michiel): Do nothing
        } break;
        
        case Expr_Unary:
        {
            result = collapse_parenthesis(optimizer, expr->unary.expr);
        } break;
        
        case Expr_Binary:
        {
            Expr *left = expr->binary.left;
            Expr *right = expr->binary.right;
            OpPrecedence opP = get_op_precedence(expr->binary.op);
                if ((left->kind == Expr_Paren) &&
                (left->paren.expr->kind == Expr_Binary))
            {
                OpPrecedence leftOpP = get_op_precedence(left->paren.expr->binary.op);
                if ((opP.level < leftOpP.level) ||
                    ((opP.level == leftOpP.level) && 
                     (opP.commutative ||
                     ((opP.associate == Associate_LeftToRight) &&
                       (leftOpP.associate == Associate_LeftToRight)))))
                {
                    expr->binary.left = left->paren.expr;
                    free_expr(optimizer, left);
                result = true;
            }
                }
            if ((right->kind == Expr_Paren) &&
                (right->paren.expr->kind == Expr_Binary))
            {
                OpPrecedence rightOpP = get_op_precedence(right->paren.expr->binary.op);
                if ((opP.level < rightOpP.level) ||
                    ((opP.level == rightOpP.level) &&
                     (opP.commutative ||
                     ((opP.associate == Associate_RightToLeft) &&
                       (rightOpP.associate == Associate_RightToLeft)))))
                {
                    expr->binary.right = right->paren.expr;
                    free_expr(optimizer, right);
                result = true;
            }
                }
            
            result |= collapse_parenthesis(optimizer, expr->binary.left);
            result |= collapse_parenthesis(optimizer, expr->binary.right);
        } break;
        
        INVALID_DEFAULT_CASE;
    }
    
    return result;
}

global Map gConstSymbols_;
global Map *gConstSymbols = &gConstSymbols_;

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
            // NOTE(michiel): If last time it was assigned a constant value
            Expr *constant = map_get(gConstSymbols, expr->name.data);
            if (constant)
            {
                expr->kind = Expr_Int;
                expr->intConst = constant->intConst;
            }
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
            #if 0
        // NOTE(michiel): Swap IO to be as far left as possible for constant optimizations
            if ((expr->binary.right->kind == Expr_Id) &&
                strings_are_equal(expr->binary.right->name, create_string("IO")) &&
                get_op_precedence(expr->binary.op).commutative)
            {
                Expr *temp = expr->binary.right;
                expr->binary.right = expr->binary.left;
                expr->binary.left = temp;
            }
            #endif

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
            else if (expr->binary.left->kind == Expr_Int)
            {
                Expr *right = expr->binary.right;
                
                TokenKind op = expr->binary.op;
                OpPrecedence opP = get_op_precedence(op);
                
                if (right->kind == Expr_Binary)
                    {
                    TokenKind rightOp = right->binary.op;
                    OpPrecedence rightOpP = get_op_precedence(rightOp);
                    
                    if ((right->binary.left->kind == Expr_Int) ||
                     (rightOpP.commutative && 
                      (right->binary.right->kind == Expr_Int) &&
                      (right->binary.left->kind == Expr_Id) &&
                      strings_are_equal(right->binary.left->name, create_string("IO"))))
                {
                    if (right->binary.left->kind == Expr_Id)
                        {
                            // NOTE(michiel): Swap IO so we can optimize further
                        i_expect(rightOpP.commutative);
                        Expr *temp = right->binary.left;
                        right->binary.left = right->binary.right;
                        right->binary.right = temp;
                    }
                    
                    s64 leftVal = expr->binary.left->intConst;;
                    s64 rightVal = right->binary.left->intConst;
                    s64 val = 0;
                    b32 update = false;
                    
                    // NOTE(michiel): If the right operator is of lesser precedence than the
                    // current or they are the same and commutative or they are the same and
                    // it is left associative.
                    // TODO(michiel): Right associativity handling
                    b32 execute = ((rightOpP.level < opP.level) ||
                                   ((opP.op == rightOpP.op) && 
                                    (opP.commutative || 
                                     (opP.associate == Associate_LeftToRight))));
                    
                    if (execute)
                    {
                        val = execute_op(op, leftVal, rightVal);
                        update = true;
                    }
                    else if (((op == '+') || (op == '-')) && 
                             ((rightOp == '+') || (rightOp == '-')))
                    {
                        // NOTE(michiel): If we got X - 2 + 3 we can combine it to X + 1
                        // X - 2 + 3 => X + (-2 + 3) => X + (3 - 2) => X - -(3 - 2)
                        // X - (2 - 3) => X - -1
                        
                        if (((rightOp == '+') && (op == '-')) ||
                            ((rightOp == '-') && (op == '+')))
                        {
                            val = leftVal - rightVal;
                            update = true;
                        }
                        else if ((rightOp == '-') && (op == '-'))
                        {
                            val = leftVal + rightVal;
                            update = true;
                        }
                        
                        if (update && (val < 0))
                        {
                            // NOTE(michiel): Switch op and negate
                            op = (op == '+') ? '-' : '+';
                            val = -val;
                        }
                    }
                    
                    if (update)
                    {
                        expr->binary.op = op;
                        expr->binary.left->intConst = val;
                        expr->binary.right = right->binary.right;
                        free_expr(optimizer, right->binary.left);
                        free_expr(optimizer, right);
                    }
                }
}
            } 
            else if (expr->binary.right->kind == Expr_Int)
            {
                Expr *left = expr->binary.left;
                
                TokenKind op = expr->binary.op;
                OpPrecedence opP = get_op_precedence(op);
                
    if (left->kind == Expr_Binary)
    {
    TokenKind leftOp = left->binary.op;
                OpPrecedence leftOpP = get_op_precedence(leftOp);

                if ((left->binary.right->kind == Expr_Int) ||
                     (leftOpP.commutative && 
                      (left->binary.left->kind == Expr_Int) &&
                      (left->binary.right->kind == Expr_Id) &&
                      strings_are_equal(left->binary.right->name, create_string("IO"))))
                {
                    if (left->binary.right->kind == Expr_Id)
                    {
                        i_expect(leftOpP.commutative);
                        Expr *temp = left->binary.left;
                        left->binary.left = left->binary.right;
                        left->binary.right = temp;
                    }
    
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
}
        } break;
        
        INVALID_DEFAULT_CASE;
    }
    
    if (collapse_parenthesis(optimizer, expr))
    {
        combine_const(optimizer, expr);
    }
}

internal void
set_usage(Map *usedVars, Expr *expr)
{
    switch (expr->kind)
    {
        case Expr_Paren:
        {
            set_usage(usedVars, expr->paren.expr);
        } break;
        
        case Expr_Int:
        {
            // NOTE(michiel): Do nothing
        } break;
        
        case Expr_Id:
        {
            map_put_u64(usedVars, expr->name.data, 1);
        } break;
        
        case Expr_Unary:
        {
            set_usage(usedVars, expr->unary.expr);
        } break;
        
        case Expr_Binary:
        {
            set_usage(usedVars, expr->binary.left);
            set_usage(usedVars, expr->binary.right);
        } break;
        
        INVALID_DEFAULT_CASE;
    }
}

internal void
copy_expr(AstOptimizer *optimizer, Expr *source, Expr *dest)
{
    dest->kind = source->kind;
    
    switch (dest->kind)
    {
        case Expr_Paren:
        {
            dest->paren.expr = ast_alloc_expr(optimizer);
            copy_expr(optimizer, source->paren.expr, dest->paren.expr);
        } break;
        
        case Expr_Int:
        {
            dest->intConst = source->intConst;
        } break;
        
        case Expr_Id:
        {
            dest->name = source->name;
        } break;
        
        case Expr_Unary:
        {
            dest->unary.op = source->unary.op;
            dest->unary.expr = ast_alloc_expr(optimizer);
            copy_expr(optimizer, source->unary.expr, dest->unary.expr);
        } break;
        
        case Expr_Binary:
        {
            dest->binary.op = source->binary.op;
            dest->binary.left = ast_alloc_expr(optimizer);
            dest->binary.right = ast_alloc_expr(optimizer);
            copy_expr(optimizer, source->binary.left, dest->binary.left);
            copy_expr(optimizer, source->binary.right, dest->binary.right);
        } break;
        
        INVALID_DEFAULT_CASE;
    }
}

internal void
ast_optimize(AstOptimizer *optimizer)
{
    // NOTE(michiel): Insert unique variable names
    for (u32 stmtIdx = 0; stmtIdx < optimizer->statements.stmtCount; ++stmtIdx)
    {
        Stmt *stmt = optimizer->statements.stmts[stmtIdx];
        if (stmt->kind == Stmt_Assign)
        {
            collapse_parenthesis(optimizer, stmt->assign.left);
            collapse_parenthesis(optimizer, stmt->assign.right);
            
            i_expect(stmt->assign.left->kind == Expr_Id);
            assign_var_names(optimizer, stmt->assign.left, true);
            assign_var_names(optimizer, stmt->assign.right, false);
             while (stmt->assign.right->kind == Expr_Paren)
            {
                // NOTE(michiel): Remove extra parenthesized assignments
                Expr *removal = stmt->assign.right;
                stmt->assign.right = stmt->assign.right->paren.expr;
                free_expr(optimizer, removal);
            }
            
            // NOTE(michiel): Copy over expressions if this assignment has a 
            // single var as rhs
            if ((stmt->assign.right->kind == Expr_Id) &&
                !strings_are_equal(stmt->assign.right->name, create_string("IO")))
            {
                Expr *expr = map_get(gAstSymbolExpr, stmt->assign.right->name.data);
            i_expect(expr);
                copy_expr(optimizer, expr, stmt->assign.right);
                }
            map_put(gAstSymbolExpr, stmt->assign.left->name.data,
                    stmt->assign.right);
            }
        else
        {
            i_expect(stmt->kind == Stmt_Hint);
        }
    }
    
    // NOTE(michiel): Combine constants and record assignments of constants
    for (u32 stmtIdx = 0; stmtIdx < optimizer->statements.stmtCount;)
    {
        Stmt *stmt = optimizer->statements.stmts[stmtIdx];
        if (stmt->kind == Stmt_Assign)
        {
            combine_const(optimizer, stmt->assign.left);
            combine_const(optimizer, stmt->assign.right);
            if (stmt->assign.right->kind == Expr_Int)
            {
                i_expect(stmt->assign.left->kind == Expr_Id);
                if (!is_key_word(stmt->assign.left->name))
                {
                map_put(gConstSymbols, stmt->assign.left->name.data,
                        stmt->assign.right);
                    for (s32 moveIdx = stmtIdx;
                         moveIdx < (optimizer->statements.stmtCount - 1);
                         ++moveIdx)
                    {
                        optimizer->statements.stmts[moveIdx] = optimizer->statements.stmts[moveIdx + 1];
                    }
                    --optimizer->statements.stmtCount;
                }
                else
                {
                    ++stmtIdx;
                }
            }
            else
            {
                ++stmtIdx;
            }
            }
        else
        {
            combine_const(optimizer, stmt->expr);
            ++stmtIdx;
        }
    }
    
    Map usedVars = {0};
    for (s32 stmtIdx = optimizer->statements.stmtCount - 1; stmtIdx >= 0; --stmtIdx)
    {
        Stmt *stmt = optimizer->statements.stmts[stmtIdx];
        if (stmt->kind == Stmt_Assign)
        {
            i_expect(stmt->assign.left->kind == Expr_Id);
            b32 isUsed = false;
            if (!strings_are_equal(stmt->assign.left->name, create_string("IO")))
            {
                u64 usage = map_get_u64(&usedVars, stmt->assign.left->name.data);
                isUsed = (usage != 0);
                    }
            else
            {
                isUsed = true;
            }
            
            if (isUsed)
            {
                set_usage(&usedVars, stmt->assign.right);
                          }
            else
            {
                Expr *expr = stmt->assign.left;
                
                fprintf(stderr, "%.*s:%d:%d: Unused variable %.*s.\n",
                        expr->origin.filename.size, expr->origin.filename.data,
                        expr->origin.lineNumber, expr->origin.colNumber,
                        expr->name.size, expr->name.data);
                
                free_all_expr(optimizer, stmt->assign.left);
                free_all_expr(optimizer, stmt->assign.right);
                
                for (u32 moveIdx = stmtIdx;
                     moveIdx < optimizer->statements.stmtCount - 1; 
                     ++moveIdx)
                {
                    optimizer->statements.stmts[moveIdx] = optimizer->statements.stmts[moveIdx + 1];
                }
                buf_pop(optimizer->statements.stmts);
                --optimizer->statements.stmtCount;
                free_stmt(optimizer, stmt);
            }
        }
        else
        {
            i_expect(stmt->kind == Stmt_Hint);
        }
    }
}
