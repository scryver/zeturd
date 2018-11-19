global char *gUnaryNames[NUM_TOKENS] = 
{
    ['+'] = "pos",
    ['-'] = "neg",
    ['~'] = "inv",
    ['!'] = "not",
    [TOKEN_INC] = "++",
    [TOKEN_DEC] = "--",
};

internal inline String
get_unary_name(TokenKind op)
{
    i_expect(op < NUM_TOKENS);
    String result = str_internalize_cstring(gUnaryNames[op]);
    return result;
}

global char *gBinaryNames[NUM_TOKENS] = 
{
    [TOKEN_POW] = "pow",
    ['*'] = "mul",
    ['/'] = "div",
    [TOKEN_SLL] = "shift-left",
    [TOKEN_SRA] = "shift-right-arith",
    [TOKEN_SRL] = "shift-right-logic",
    ['+'] = "add",
    ['-'] = "sub",
    ['&'] = "and",
    ['^'] = "xor",
    ['|'] = "or",
    ['='] = ":=",
};

internal inline String
get_binary_name(TokenKind op)
{
    i_expect(op < NUM_TOKENS);
    String result = str_internalize_cstring(gBinaryNames[op]);
    return result;
}

global u64 gTempCount = 0;

internal inline String
get_temporary_name(void)
{
    String result = create_string_fmt("turd%d", ++gTempCount);
    return result;
}

internal inline b32
is_flat_expr(Expr *expr)
{
    b32 result = false;
    if ((expr->kind == Expr_Id) ||
        (expr->kind == Expr_Int))
    {
        result = true;
    }
    return result;
}

internal String
generate_ir_expr(AstOptimizer *optimizer, Expr *expr, FileStream output, 
                 Expr *parent)
{
    String result = {0};
    switch (expr->kind)
    {
        case Expr_Paren:
        {
             String paren = generate_ir_expr(optimizer, expr->paren.expr, output, expr);
            result = paren; // create_string_fmt("(%.*s)", paren.size, paren.data);
        } break;
        
        case Expr_Int:
        {
            result = create_string_fmt("%ld", expr->intConst);
        } break;
        
        case Expr_Id:
        {
            result = expr->name;
        } break;
        
        case Expr_Unary:
        {
            String opStr = get_unary_name(expr->unary.op);
            String operand = generate_ir_expr(optimizer, expr->unary.expr, output, expr);
            
            if (parent)
            {
                String temp = get_temporary_name();
                String assignOp = get_binary_name(TOKEN_ASSIGN);
                fprintf(output.file, "%.*s %.*s %.*s %.*s\n", temp.size, temp.data,
                        assignOp.size, assignOp.data, opStr.size, opStr.data, 
                        operand.size, operand.data);
                result = temp;
            }
            else
            {
                result = create_string_fmt("%.*s %.*s", opStr.size, opStr.data, 
                                           operand.size, operand.data);
            }
        } break;
        
        case Expr_Binary:
        {
            String opStr = get_binary_name(expr->binary.op);
            String left = generate_ir_expr(optimizer, expr->binary.left, output, expr);
            String right = generate_ir_expr(optimizer, expr->binary.right, output, expr);
            
            if (parent)
            {
                String temp = get_temporary_name();
                String assignOp = get_binary_name(TOKEN_ASSIGN);
                fprintf(output.file, "%.*s %.*s %.*s %.*s %.*s\n", temp.size, temp.data,
                        assignOp.size, assignOp.data, left.size, left.data,
                        opStr.size, opStr.data, right.size, right.data);
                        result = temp;
            }
            else
            {
            result = create_string_fmt("%.*s %.*s %.*s", left.size, left.data,
                                       opStr.size, opStr.data, right.size, right.data);
            }
        } break;
        
        INVALID_DEFAULT_CASE;
    }
    return result;
}

internal void
generate_ir(AstOptimizer *optimizer, FileStream output)
{
    for (u32 stmtIdx = 0; stmtIdx < optimizer->statements.stmtCount; ++stmtIdx)
    {
        Stmt *stmt = optimizer->statements.stmts[stmtIdx];
        if (stmt->kind == Stmt_Assign)
        {
            String leftOp = generate_ir_expr(optimizer, stmt->assign.left, output, 0);
            String assignOp = get_binary_name(stmt->assign.op);
            String rightOp = generate_ir_expr(optimizer, stmt->assign.right, output, 0);
            fprintf(output.file, "%.*s %.*s %.*s;\n", leftOp.size, leftOp.data, 
                    assignOp.size, assignOp.data, rightOp.size, rightOp.data);
        }
        else
        {
            String hint = generate_ir_expr(optimizer, stmt->expr, output, 0);
            fprintf(output.file, "/* Hint:\n%.*s\n*/\n\n", hint.size, hint.data);
        }
    }
}

internal void
generate_ir_file(AstOptimizer *optimizer, char *fileName)
{
    FileStream f = {0};
    f.file = fopen(fileName, "wb");
    if (f.file)
    {
        generate_ir(optimizer, f);
    fclose(f.file);
}
    else
    {
        // TODO(michiel): Error
    }
    }
