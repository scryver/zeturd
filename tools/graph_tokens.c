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
graph_token_expr4(TokenGraph *graph, Token **token, String connection)
{
    String result = {0};
    if ((*token)->kind == TOKEN_LINE_COMMENT)
    {
        // NOTE(michiel): Do nothing
        *token = (*token)->nextToken;
    }
    else if ((*token)->kind == TOKEN_NUMBER)
    {
        // NOTE(michiel): Calc something
        String number = (*token)->value;
        if ((number.size > 1) && (number.data[0] == '0'))
        {
            if ((number.data[1] == 'b') || (number.data[1] == 'B'))
            {
                number = create_string_fmt("bin%.*s%d", 
                                           (*token)->value.size, (*token)->value.data,
                                           graph->id++);
            }
            else if ((number.data[1] == 'x') || (number.data[1] == 'X'))
            {
                number = create_string_fmt("hex%.*s%d", 
                                           (*token)->value.size, (*token)->value.data,
                                           graph->id++);
            }
            else
            {
                number = create_string_fmt("oct%.*s%d", 
                                           (*token)->value.size, (*token)->value.data,
                                           graph->id++);
            }
        }
        else
        {
            number = create_string_fmt("%.*s%d", (*token)->value.size, (*token)->value.data,
                                graph->id++);
        }
        
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
graph_token_expr3(TokenGraph *graph, Token **token, String connection)
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
        graph_token_expr4(graph, token, minus);
        result = minus;
    }
    else
    {
        result = graph_token_expr4(graph, token, connection);
    }
    
    return result;
}

internal String
graph_token_expr2(TokenGraph *graph, Token **token, String connection)
{
    String left = graph_token_expr3(graph, token, (String){0,0});
    String op = left;
    if (left.size)
    {
    while ((*token)->kind == TOKEN_POW)
    {
        op = create_string_fmt("op%d", graph->id++);
        fprintf(graph->output.file, "  %.*s [label=\"**\"];\n", op.size, op.data);
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
    }
    
    return op;
}

internal String
graph_token_expr1(TokenGraph *graph, Token **token, String connection)
{
    String left = graph_token_expr2(graph, token, (String){0,0});
    String op = left;
    
    if (left.size)
    {
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
    }
    
    return op;
}

internal String
graph_token_expr0(TokenGraph *graph, Token **token, String connection)
{
    String left = graph_token_expr1(graph, token, (String){0,0});
    String op = left;
    
    if (left.size)
    {
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
