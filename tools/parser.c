global Map gIdentifierMap_ = {0};
global Map *gIdentifierMap = &gIdentifierMap_;

internal Constant *
parse_constant(Token **at)
{
    i_expect((*at)->kind == TOKEN_NUMBER);
    Constant *result = allocate_struct(Constant, 0);
    result->value = (u32)string_to_number((*at)->value);
    *at = (*at)->nextToken;
    return result;
}

internal Identifier *
parse_identifier(Token **at)
{
    i_expect((*at)->kind == TOKEN_ID);
    String id = str_internalize((*at)->value);
    // NOTE(michiel): Check if we already got this identifier
    Identifier *result = map_get(gIdentifierMap, id.data);

    if (!result)
    {
        result = allocate_struct(Identifier, 0);
        result->name = id;
        map_put(gIdentifierMap, result->name.data, result);
    }
    *at = (*at)->nextToken;
    return result;
}

typedef struct ParseVar
{
    b32 isExpr;
    union
    {
        Variable *var;
        Expression *expr;
    };
} ParseVar;

internal Expression *parse_expression_add_op(Token **at, Expression *leftExpr);

internal ParseVar
parse_variable(Token **at)
{
    ParseVar result = {0};

    if ((*at)->kind == TOKEN_ID)
    {
        result.var = allocate_struct(Variable, 0);
        result.var->kind = VARIABLE_IDENTIFIER;
        result.var->id = parse_identifier(at);
    }
    else if ((*at)->kind == TOKEN_NUMBER)
    {
        result.var = allocate_struct(Variable, 0);
        result.var->kind = VARIABLE_CONSTANT;
        result.var->constant = parse_constant(at);
    }
    else if ((*at)->kind == '(')
    {
        *at = (*at)->nextToken;
        result.isExpr = true;
        result.expr = parse_expression_add_op(at, 0);
        result.expr->complete = true;
        i_expect((*at)->kind == ')');
        *at = (*at)->nextToken;
    }
    else
    {
        fprintf(stderr, "Unknown variable token: ");
        print_token((FileStream){.file=stderr}, *at);
        fprintf(stderr, "\n");
    }
    return result;
}

// TODO(michiel): Clean up handling, so this isn't needed to get rid of empty results
internal inline Expression *
dust_off_expression(Expression *expr)
{
    if (expr)
    {
        while ((expr->op == EXPR_OP_NOP) &&
               (expr->leftKind == EXPRESSION_EXPR))
        {
            i_expect(expr->rightKind == EXPRESSION_NULL);
            i_expect(expr->rightExpr == 0);
            Expression *remove = expr;
            expr = expr->leftExpr;
            deallocate(remove);
        }
    }

    return expr;
}

internal inline Expression *
new_op_has_precedence(Expression *newOp, Expression *oldOp)
{
    while ((oldOp->rightKind == EXPRESSION_EXPR) &&
           (!oldOp->rightExpr->complete) &&
           (oldOp->rightExpr->op > newOp->op))
    {
        oldOp = oldOp->rightExpr;
    }
    newOp->left = oldOp->right;
    newOp->leftKind = oldOp->rightKind;
    i_expect(newOp->rightKind == EXPRESSION_VAR);
    oldOp->rightExpr = newOp;
    oldOp->rightKind = EXPRESSION_EXPR;
    return oldOp;
}

internal inline Expression *
old_op_has_precedence(Expression *newOp, Expression *oldOp)
{
    newOp->leftExpr = oldOp;
    newOp->leftKind = EXPRESSION_EXPR;
    return newOp;
}

internal inline Expression *
parse_expression_precedence(Token **at, Expression *curExpr, Expression *leftExpr, ExpressionOp op)
{
    Expression *result = curExpr;
    result->op = op;
    *at = (*at)->nextToken;
    ParseVar right = parse_variable(at);
    if (right.isExpr)
    {
        result->rightExpr = right.expr;
        result->rightKind = EXPRESSION_EXPR;
    }
    else
    {
        result->right = right.var;
        result->rightKind = EXPRESSION_VAR;
    }

    result = dust_off_expression(result);
    leftExpr = dust_off_expression(leftExpr);

    if (leftExpr)
    {
        if (leftExpr->op <= result->op)
        {
            result = old_op_has_precedence(result, leftExpr);
        }
        else
        {
            i_expect(leftExpr->op != EXPR_OP_NOP);
            result = new_op_has_precedence(result, leftExpr);
        }
    }

    return result;
}

#define CASEC(c, x) case c: { result = parse_expression_precedence(at, result, leftExpr, EXPR_OP_##x); } break

#define CASET(x) case TOKEN_##x: { result = parse_expression_precedence(at, result, leftExpr, EXPR_OP_##x); } break

internal Expression *
parse_expression_mul_op(Token **at, Expression *leftExpr)
{
    b32 done = false;
    Expression *result;

    if (!leftExpr)
    {
        ParseVar left = parse_variable(at);
        if (left.isExpr)
        {
            result = left.expr;
        }
        else
        {
            result = allocate_struct(Expression, 0);
            result->op = EXPR_OP_NOP;
            result->left = left.var;
            result->leftKind = EXPRESSION_VAR;
        }
    }
    else
    {
        result = allocate_struct(Expression, 0);
        result->op = EXPR_OP_NOP;
    }

    switch ((u32)(*at)->kind)
    {
        CASEC('*', MUL);
        CASEC('/', DIV);
        CASEC('&', AND);
        CASET(SLL);
        CASET(SRL);
        CASET(SRA);
        default:
        {
            if (leftExpr)
            {
                result = leftExpr;
            }
            done = true;
        } break;
    }

    if (!done &&
        ((EXPR_OP_MUL <= result->op) && (result->op <= EXPR_OP_SRA)))
    {
        result = parse_expression_mul_op(at, result);
    }

    return result;
}

internal Expression *
parse_expression_add_op(Token **at, Expression *leftExpr)
{
    Expression *result = allocate_struct(Expression, 0);
    b32 done = false;
    result->op = EXPR_OP_NOP;
    if (!leftExpr)
    {
        result->leftExpr = parse_expression_mul_op(at, 0);
        result->leftKind = EXPRESSION_EXPR;
        if (result->leftExpr->op == EXPR_OP_NOP)
        {
            Expression *leftE = result->leftExpr;

            result->left = leftE->left;
            result->leftKind = leftE->leftKind;
            deallocate(leftE);
        }
    }
    else
    {
        result->leftExpr = leftExpr;
        result->leftKind = EXPRESSION_EXPR;
    }

    switch ((u32)(*at)->kind)
    {
        CASEC('-', SUB);
        CASEC('+', ADD);
        CASEC('|', OR);
        CASEC('^', XOR);
        default:
        {
            result = parse_expression_mul_op(at, result);
            if (result->op == EXPR_OP_NOP)
            {
                done = true;
            }
        } break;
    }

    if (!done &&
        (EXPR_OP_MUL <= result->op) &&
        (result->op <= EXPR_OP_XOR))
    {
        result = parse_expression_add_op(at, result);
    }

    return result;
}

#undef CASEC
#undef CASET

internal Expression *
parse_expression(Token **at)
{
    Expression *result = parse_expression_add_op(at, 0);
    if ((result->op == EXPR_OP_NOP) &&
        (result->leftKind == EXPRESSION_EXPR))
    {
        result = result->leftExpr;
    }
    return result;
}

internal Assignment *
parse_assignment(Token **at)
{
    Assignment *result = allocate_struct(Assignment, 0);
    result->id = parse_identifier(at);
    if ((*at)->kind != '=')
    {
        fprintf(stderr, "ASSIGN expected, got ");
        print_token((FileStream){.file=stderr}, *at);
        fprintf(stderr, "\n");
    }
    i_expect((*at)->kind == '=');
    *at = (*at)->nextToken;
    result->expr = parse_expression(at);
    return result;
}

internal void
parse_statement(Token **at, Statement *statement)
{
    if ((*at)->nextToken && ((*at)->nextToken->kind == '='))
    {
        statement->kind = STATEMENT_ASSIGN;
        statement->assign = parse_assignment(at);
    }
    else
    {
        statement->kind = STATEMENT_EXPR;
        statement->expr = parse_expression(at);
    }
}

#define IS_END_STATEMENT(token) ((token->kind == TOKEN_EOF) || (token->kind == '\n') || (token->kind == ';'))

internal Program *
parse(Token *tokens)
{
    Program *program = allocate_struct(Program, 0);

    Token *at = tokens;
    while (at)
    {
        i_expect(program->nrStatements < MAX_NR_STATEMENTS);
        parse_statement(&at, program->statements + program->nrStatements++);

        do
        {
            if (!IS_END_STATEMENT(at))
            {
                fprintf(stderr, "Statement not closed by newline or semi-colon!\nStuck at: ");
                print_token((FileStream){.file=stderr}, at);
                fprintf(stderr, "\n");
            }
            i_expect(IS_END_STATEMENT(at));
            at = at->nextToken;
        }
        while (at && IS_END_STATEMENT(at));
    }

    return program;
}

#undef IS_END_STATEMENT

internal void
print_constant(FileStream stream, Constant *constant)
{
    char *format = "%d";
    if (stream.verbose)
    {
        format = "[const %d]";
    }
    fprintf(stream.file, format, constant->value);
}

internal void
print_identifier(FileStream stream, Identifier *id)
{
    char *format = "'%.*s'";
    if (stream.verbose)
    {
        format = "[id '%.*s']";
    }
    fprintf(stream.file, format, id->name.size, (char *)id->name.data);
}

internal void
print_variable(FileStream stream, Variable *var)
{
    if (stream.verbose)
    {
        fprintf(stream.file, "[var ");
    }
    if (!var)
    {
        fprintf(stream.file, "EMPTY_VAR");
    }
    else
    {
        switch (var->kind)
        {
            case VARIABLE_NULL:
            {
                // TODO(michiel): Error?
            } break;

            case VARIABLE_IDENTIFIER:
            {
                print_identifier(stream, var->id);
            } break;

            case VARIABLE_CONSTANT:
            {
                print_constant(stream, var->constant);
            } break;

            default:
            {
                fprintf(stream.file, "INVALID");
            } break;
        }
    }
    if (stream.verbose)
    {
        fprintf(stream.file, "]");
    }
}

internal void
print_expression_op(FileStream stream, Expression *expr, char *opstr)
{
    fprintf(stream.file, "(%s ", opstr);
    if (expr->leftKind == EXPRESSION_VAR)
    {
        print_variable(stream, expr->left);
    }
    else
    {
        i_expect(expr->leftKind == EXPRESSION_EXPR);
        print_expression(stream, expr->leftExpr);
    }

    fprintf(stream.file, " ");

    if (expr->rightKind == EXPRESSION_VAR)
    {
        print_variable(stream, expr->right);
    }
    else
    {
        i_expect(expr->rightKind == EXPRESSION_EXPR);
        print_expression(stream, expr->rightExpr);
    }
    fprintf(stream.file, ")");
}

#define CASE(name, printName) case EXPR_OP_##name: { print_expression_op(stream, expr, #printName); } break

internal void
print_expression(FileStream stream, Expression *expr)
{
    if (stream.verbose)
    {
        fprintf(stream.file, "[expr ");
    }
    switch (expr->op)
    {
        case EXPR_OP_NOP:
        {
            if (expr->leftKind == EXPRESSION_VAR)
            {
                print_variable(stream, expr->left);
            }
            else if (expr->leftKind == EXPRESSION_EXPR)
            {
                print_expression(stream, expr->leftExpr);
            }
            else
            {
                fprintf(stream.file, "EMPTY");
            }
        } break;

        CASE(MUL, mul);
        CASE(DIV, div);
        CASE(AND, and);
        CASE(SLL, sll);
        CASE(SRL, srl);
        CASE(SRA, sra);
        CASE(SUB, sub);
        CASE(ADD, add);
        CASE(OR, or);
        CASE(XOR, xor);

        INVALID_DEFAULT_CASE;
    }
    if (stream.verbose)
    {
        fprintf(stream.file, "]");
    }
}

#undef CASE

internal void
print_assignment(FileStream stream, Assignment *assign)
{
    fprintf(stream.file, "(assign ");
    print_identifier(stream, assign->id);
    fprintf(stream.file, " ");
    print_expression(stream, assign->expr);
    fprintf(stream.file, ")");
}

internal void
print_statement(FileStream stream, Statement *statement)
{
    if (stream.verbose)
    {
        fprintf(stream.file, "[stmt ");
    }
    switch (statement->kind)
    {
        case STATEMENT_ASSIGN: print_assignment(stream, statement->assign); break;
        case STATEMENT_EXPR:   print_expression(stream, statement->expr); break;
        INVALID_DEFAULT_CASE;
    }
    if (stream.verbose)
    {
        fprintf(stream.file, "]");
    }
}

internal void
print_parsed_program(FileStream stream, Program *program)
{
    if (stream.verbose)
    {
        fprintf(stream.file, "[program \n  ");
    }
    for (u32 statementIndex = 0;
         statementIndex < program->nrStatements;
         ++statementIndex)
    {
        Statement *statement = program->statements + statementIndex;
        print_statement(stream, statement);
        if (statementIndex < (program->nrStatements - 1))
        {
            fprintf(stream.file, "\n%s", stream.verbose ? "  " : "");
        }
    }
    if (stream.verbose)
    {
        fprintf(stream.file, "]\n");
    }
    else
    {
        fprintf(stream.file, "\n");
    }
}
