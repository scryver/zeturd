internal void
graph_connect(FileStream output, String from, String to)
{
    fprintf(output.file, "    %.*s -> %.*s;\n", from.size, from.data,
            to.size, to.data);
}

internal void
graph_label(FileStream output, String name, char *labelFmt, ...)
{
    fprintf(output.file, "    %.*s [label=\"", name.size, name.data);
    
    va_list args;
    va_start(args, labelFmt);
    vfprintf(output.file, labelFmt, args);
    va_end(args);
    
    fprintf(output.file, "\"];\n");
}

internal void
graph_ast_expression(FileStream output, Expr *expr, String connection)
{
    switch (expr->kind)
    {
        case Expr_Paren:
        {
            String paren = create_string_fmt("paren%p", expr);
            graph_label(output, paren, "(  )");
            graph_connect(output, connection, paren);
            graph_ast_expression(output, expr->paren.expr, paren);
        } break;
        
        case Expr_Int:
        {
            String intStr = create_string_fmt("int%p", expr);
            graph_label(output, intStr, "%lld", expr->intConst);
            graph_connect(output, connection, intStr);
        } break;
        
        case Expr_Id:
        {
            String idStr = create_string_fmt("id%p", expr);
            graph_label(output, idStr, "%.*s", expr->name.size, expr->name.data);
            graph_connect(output, connection, idStr);
        } break;
        
        case Expr_Unary:
        {
            String opStr = create_string_fmt("unOp%p", expr);
            if (expr->unary.op == TOKEN_INC)
            {
                graph_label(output, opStr, "++");
            }
            else if (expr->unary.op == TOKEN_DEC)
            {
                graph_label(output, opStr, "--");
            }
            else
            {
                graph_label(output, opStr, "%c", expr->unary.op);
            }
            graph_connect(output, connection, opStr);
            graph_ast_expression(output, expr->unary.expr, opStr);
        } break;
        
        case Expr_Binary:
        {
            String opStr = create_string_fmt("binOp%p", expr);
            if (expr->binary.op == TOKEN_POW)
            {
                graph_label(output, opStr, "**");
            }
            else if (expr->binary.op == TOKEN_SLL)
            {
                graph_label(output, opStr, "<<");
            }
            else if (expr->binary.op == TOKEN_SRA)
            {
                graph_label(output, opStr, ">>");
            }
            else if (expr->binary.op == TOKEN_SRL)
            {
                graph_label(output, opStr, ">>>");
            }
            else
            {
                graph_label(output, opStr, "%c", expr->binary.op);
            }
            graph_connect(output, connection, opStr);
            graph_ast_expression(output, expr->binary.left, opStr);
            graph_ast_expression(output, expr->binary.right, opStr);
        } break;
        
        case Expr_None:
        {
            fprintf(stderr, "%.*s:%d:%d: Unexpected freed expression.\n",
                    expr->origin.filename.size, expr->origin.filename.data,
                    expr->origin.lineNumber, expr->origin.colNumber);
        } break;
        
        INVALID_DEFAULT_CASE;
    }
}

internal void
graph_ast_statement(FileStream output, Stmt *stmt, String connection)
{
    switch (stmt->kind)
    {
        case Stmt_Assign:
        {
            String opStr = create_string_fmt("assign%p", stmt);
            graph_label(output, opStr, "=");
            graph_connect(output, connection, opStr);
            graph_ast_expression(output, stmt->assign.left, opStr);
            graph_ast_expression(output, stmt->assign.right, opStr);
        } break;
        
        case Stmt_Hint:
        {
            graph_ast_expression(output, stmt->expr, connection);
        } break;
        
        INVALID_DEFAULT_CASE;
    }
}

internal void
graph_ast(StmtList *stmts, char *fileName)
{
      FileStream output = {0};
    output.file = fopen(fileName, "wb");
    
    fprintf(output.file, "digraph ast {\n");
    for (u32 stmtIdx = 0; stmtIdx < stmts->stmtCount; ++stmtIdx)
    {
        Stmt *stmt = stmts->stmts[stmtIdx];
        String connection = create_string_fmt("stmt%d", stmtIdx + 1);
        fprintf(output.file, "  subgraph cluster%d {\n", stmtIdx + 1);
        graph_ast_statement(output, stmt, connection);
        fprintf(output.file, "  }\n");
    }
    fprintf(output.file, "}\n\n");
    fclose(output.file);
}

#if 0
internal inline b32
expect_token_kind(Token *token, u32 expectedKind)
{
    b32 error = false;
    if (token->kind != expectedKind)
    {
        fprintf(stderr, "Token did not match expected token!\n");
        error = true;
    }
    return error;
}

typedef struct TokenGraph
{
    FileStream output;
    u32 id;
} TokenGraph;

internal String graph_token_expr(TokenGraph *graph, Token **token, String connection);

internal String
graph_token_expr3(TokenGraph *graph, Token **token, String connection)
{
    String result = {0};
    if ((*token)->kind == TOKEN_NUMBER)
    {
        // NOTE(michiel): Calc something
        String number = create_string_fmt("%.*s%d", (*token)->value.size, (*token)->value.data,
                                          graph->id++);
        fprintf(graph->output.file, "  %.*s [label=\"%.*s\"];\n", number.size, number.data,
                (*token)->value.size, (*token)->value.data);
        if (connection.size)
        {
            fprintf(graph->output.file, "  %.*s -> %.*s;\n", connection.size, connection.data,
                    number.size, number.data);
        }
        *token = (*token)->nextToken;
        
        result = number;
    }
    else if ((*token)->kind == TOKEN_ID)
    {
        // NOTE(michiel): Store/load the var
        String id = create_string_fmt("%.*s%d", (*token)->value.size, (*token)->value.data,
                                      graph->id++);
        fprintf(graph->output.file, "  %.*s [label=\"%.*s\"];\n", id.size, id.data,
                (*token)->value.size, (*token)->value.data);
        if (connection.size)
        {
            fprintf(graph->output.file, "  %.*s -> %.*s;\n", connection.size, connection.data,
                    id.size, id.data);
        }
        *token = (*token)->nextToken;
        
        result = id;
    }
    else if ((*token)->kind == '(')
    {
        String paren = create_string_fmt("paren%d", graph->id++);
        fprintf(graph->output.file, "  %.*s [label=\"(  )\"];\n", paren.size, paren.data);
        
        if (connection.size)
        {
            fprintf(graph->output.file, "  %.*s -> %.*s;\n", connection.size, connection.data,
                    paren.size, paren.data);
        }
        *token = (*token)->nextToken;
        graph_token_expr(graph, token, paren);
        i_expect(expect_token_kind(*token, ')') == 0);
        *token = (*token)->nextToken;
        
        result = paren;
    }
    else
    {
        // NOTE(michiel): ERROR
        fprintf(stderr, "Expected INT, ID or (, got ");
        print_token(((FileStream){.file=stderr}), *token);
        fprintf(stderr, "\n");
        INVALID_CODE_PATH;
    }
    
    return result;
}

internal String
graph_token_expr2(TokenGraph *graph, Token **token, String connection)
{
    String result;
    if ((*token)->kind == '-')
    {
        String minus = create_string_fmt("minus%d", graph->id++);
        fprintf(graph->output.file, "  %.*s [label=\"-\"];\n", minus.size, minus.data);
        
        if (connection.size)
        {
            fprintf(graph->output.file, "  %.*s -> %.*s;\n", connection.size, connection.data,
                    minus.size, minus.data);
        }
        *token = (*token)->nextToken;
        result = graph_token_expr3(graph, token, minus);
        fprintf(graph->output.file, "  %.*s -> %.*s;\n", minus.size, minus.data, 
                result.size, result.data);
        result = minus;
    }
    else
    {
        result = graph_token_expr3(graph, token, connection);
    }
    
    return result;
}

internal String
graph_token_expr1(TokenGraph *graph, Token **token, String connection)
{
    String left = graph_token_expr2(graph, token, (String){0,0});
    String op = left;
    
    while (((*token)->kind == '*') ||
           ((*token)->kind == '/') ||
           ((*token)->kind == '&'))
    {
        op = create_string_fmt("op%d", graph->id++);
        fprintf(graph->output.file, "  %.*s [label=\"%c\"];\n", op.size, op.data,
                (*token)->kind);
        *token = (*token)->nextToken;
        graph_token_expr2(graph, token, op);
        
        fprintf(graph->output.file, "  %.*s -> %.*s;\n", op.size, op.data,
                left.size, left.data);
        left = op;
    }
    
    if (connection.size)
    {
        fprintf(graph->output.file, "  %.*s -> %.*s;\n", connection.size, connection.data,
                op.size, op.data);
    }
    
    return op;
}

internal String
graph_token_expr0(TokenGraph *graph, Token **token, String connection)
{
    String left = graph_token_expr1(graph, token, (String){0,0});
    String op = left;
    
    while (((*token)->kind == '+') ||
           ((*token)->kind == '-') ||
           ((*token)->kind == '^') ||
           ((*token)->kind == '|'))
    {
        op = create_string_fmt("op%d", graph->id++);
        fprintf(graph->output.file, "  %.*s [label=\"%c\"];\n", op.size, op.data,
                (*token)->kind);
        *token = (*token)->nextToken;
        graph_token_expr1(graph, token, op);
        
        fprintf(graph->output.file, "  %.*s -> %.*s;\n", op.size, op.data,
                left.size, left.data);
        
        left = op;
    }
    
    if (connection.size)
    {
        fprintf(graph->output.file, "  %.*s -> %.*s;\n", connection.size, connection.data,
                op.size, op.data);
    }
    
    return op;
}

internal String
graph_token_expr(TokenGraph *graph, Token **token, String connection)
{
    return graph_token_expr0(graph, token, connection);
}

internal void
graph_token_statement(TokenGraph *graph, Token **token, String connection)
{
    if ((*token)->nextToken && ((*token)->nextToken->kind) == '=')
    {
        i_expect(expect_token_kind(*token, TOKEN_ID) == 0);
        String id = create_string_fmt("%.*s%d", (*token)->value.size, (*token)->value.data,
                                      graph->id++);
        fprintf(graph->output.file, "  %.*s [label=\"%.*s =\"];\n", id.size, id.data,
                (*token)->value.size, (*token)->value.data);
        *token = (*token)->nextToken;
        i_expect(expect_token_kind(*token, '=') == 0);
        *token = (*token)->nextToken;
        fprintf(graph->output.file, "  %.*s -> %.*s;\n", connection.size, connection.data, id.size, id.data);
        connection = id;
    }
    graph_token_expr(graph, token, connection);
}

internal void
graph_tokens(Token *tokens, char *fileName)
{
    TokenGraph graph = {0};
    graph.output.file = fopen(fileName, "wb");
    
    Token *at = tokens;
    fprintf(graph.output.file, "digraph tokens {\n");
    u32 statementCount = 0;
    while (at)
    {
        String connection = create_string_fmt("stmt%d", statementCount + 1);
        fprintf(graph.output.file, "  subgraph cluster%d {\n", statementCount++);
        graph_token_statement(&graph, &at, connection);
        fprintf(graph.output.file, "  }\n");
        
        while (at && ((at->kind == TOKEN_EOF) ||
                      (at->kind == '\n') ||
                      (at->kind == ';')))
        {
            at = at->nextToken;
        }
    }
    fprintf(graph.output.file, "}\n\n");
    
    fclose(graph.output.file);
}
#endif
