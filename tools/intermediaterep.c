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
    [TOKEN_SLL] = "sll",
    [TOKEN_SRA] = "sra",
    [TOKEN_SRL] = "srl",
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
    String result = create_string_fmt("_turd%d", ++gTempCount);
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
            i_expect(stmt->kind == Stmt_Hint);
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

#if 0
typedef enum IRCodeKind
{
    IRCode_None,
    IRCode_IOOut,
    IRCode_IOIn,
    IRCode_ReadA,
    IRCode_ReadB,
    IRCode_Write,
    IRCode_Calc,
    IRCode_Immediate,
} IRCodeKind;
typedef struct IRCode
{
    IRCodeKind kind;
    
    union
{
    Selection ioOut;
        struct {
            u32 addrA;
            u32 addrB;
        } memRead;
        struct {
            Selection input;
            u32 addr;
        } memWrite;
        struct {
            AluOp op;
        Selection a;
            Selection b;
            } alu;
        s32 immediate;
    };
    
    struct IRCode *sibling;
} IRCode;

typedef struct IRProg
{
    Arena buildArena;
    u32 nextFreeWriteAddr;
    Map *regAllocMap;
    IRCode **codes;
    u32 maxRegAddress;
} IRProg;

u32 get_write_address(IRProg *prog, String var)
{
    u32 result = prog->nextFreeWriteAddr++;
    // NOTE(michiel): +1 to keep 0 as a error value.
    map_put_u64(prog->regAllocMap, var.data, result + 1);
    return result;
}

u32 get_var_address(IRProg *prog, String var)
{
    u64 result = map_get_u64(prog->regAllocMap, var.data);
    i_expect(result);
    i_expect(result < U32_MAX);
    return (u32)(result - 1);
}

internal inline void
safe_select(Selection *set, Selection to)
{
    if (!set || ((*set != Select_Zero) &&
                 (*set != to)))
    {
        fprintf(stderr, "Selection already assigned to %d.\n", *set);
        INVALID_CODE_PATH;
    }
    else
    {
        *set = to;
    }
}

internal void
gen_ir_code_alu_thru(IRProg *prog, IRCode *codeSet, Selection select)
{
    // NOTE(michiel): Adds the selection to a alu no-op if the alu is not used otherwise errors
    for (IRCode *entry = codeSet; entry; entry = entry->sibling)
    {
        if (entry->kind == IRCode_Calc)
        {
            fprintf(stderr, "Alu already assigned to a different op. Unable to proceed.\n");
            INVALID_CODE_PATH;
        }
    }
    IRCode *noOp = allocate_struct(IRCode, &prog->buildArena);
    noOp->kind = IRCode_Calc;
    noOp->alu.op = Alu_Noop;
    noOp->alu.a = select;
    noOp->alu.b = Select_Zero;
    
    noOp->sibling = codeSet->sibling;
    codeSet->sibling = noOp;
}

internal IRCode *
gen_ir_code_combine(IRProg *prog, IRCode *prev, IRCode *current)
{
    IRCode *result = current;
    if (prev)
    {
    switch (current->kind)
    {
        case IRCode_IOOut:
        {
            b32 asSibling = true;
            b32 useAluThru = false;
            
            Selection sel = Select_Zero;
            for (IRCode *entry = prev;
                 entry;
                 entry = entry->sibling)
            {
                switch (entry->kind)
                {
                    case IRCode_IOIn:      { useAluThru = true; safe_select(&sel, Select_IO); } break;
                    case IRCode_IOOut:     { asSibling = false; } break;
                    case IRCode_ReadA:     { asSibling = false; safe_select(&sel, Select_MemoryA); } break;
                    case IRCode_ReadB:     { asSibling = false; safe_select(&sel, Select_MemoryB); } break;
                    case IRCode_Write:     { useAluThru = true; safe_select(&sel, entry->memWrite.input); } break;
                    case IRCode_Calc:      { asSibling = false; safe_select(&sel, Select_Alu); } break;
                    case IRCode_Immediate: { useAluThru = true; safe_select(&sel, Select_Immediate); } break;
                        INVALID_DEFAULT_CASE;
                }
            }
            
            current->ioOut = sel;
            if (asSibling)
            {
                current->sibling = prev->sibling;
                prev->sibling = current;
                    result = prev;
            }
            else
            {
                if (useAluThru)
                {
                    // NOTE(michiel): We need to wait a cycle, so push this through the alu if possible
                    gen_ir_code_alu_thru(prog, prev, sel);
                    current->ioOut = Select_Alu;
                }
                buf_push(prog->codes, current);
            }
        } break;
        
        case IRCode_ReadA:
            {
                b32 asSibling = true;
                #define MEM_USE_READ_A 0x01
                #define MEM_USE_READ_B 0x02
                #define MEM_USE_WRITE  0x04
                u32 memUsage = 0x00;
                for (IRCode *entry = prev; entry; entry = entry->sibling)
                {
                    switch (entry->kind)
                    {
                        case IRCode_ReadA: { memUsage |= MEM_USE_READ_A; } break;
                        case IRCode_ReadB: { memUsage |= MEM_USE_READ_B; } break;
                        case IRCode_Write: { memUsage |= MEM_USE_WRITE; } break;
                        default: {} break;
                    }
                }
                
                if (memUsage)
                {
                    if ((memUsage & MEM_USE_READ_B) == MEM_USE_READ_B)
                    {
                        fprintf(stderr, "Somehow only a MemB read remains on this frame!\n");
                        asSibling = true;
                    }
                    else if (((memUsage & MEM_USE_READ_A) == MEM_USE_READ_A) ||
                             ((memUsage & MEM_USE_WRITE) == MEM_USE_WRITE))
                    {
                        // TODO(michiel): Memory address check?
                        current->kind = IRCode_ReadB;
                        current->memRead.addrB = current->memRead.addrA;
                        current->memRead.addrA = U32_MAX;
                        asSibling = true;
                    }
                    else
                    {
                        asSibling = false;
                    }
                }
                
                if (asSibling)
                {
                    current->sibling = prev->sibling;
                    prev->sibling = current;
                    result = prev;
                }
                else
                {
                    buf_push(prog->codes, current);
                }
        } break;
        
        case IRCode_ReadB:
        {
                fprintf(stderr, "Do not create IRCodes with read B, this is an optimization nightmare...\n");
                INVALID_CODE_PATH;
        } break;
        
        case IRCode_Write:
        {
            b32 asSibling = true;
            b32 useAluThru = false;
            
            Selection sel = Select_Zero;
            for (IRCode *entry = prev;
                 entry;
                 entry = entry->sibling)
            {
                switch (entry->kind)
                {
                    case IRCode_IOIn:      { useAluThru = true; safe_select(&sel, Select_IO); } break;
                    case IRCode_IOOut:     { useAluThru = true; safe_select(&sel, entry->ioOut); } break;
                    // TODO(michiel): Handle read/write
                    case IRCode_ReadA:     { asSibling = false; } break;
                    case IRCode_ReadB:     { } break;
                    case IRCode_Write:     { asSibling = false; } break;
                    case IRCode_Calc:      { asSibling = false; safe_select(&sel, Select_Alu); } break;
                        case IRCode_Immediate: { useAluThru = true; safe_select(&sel, Select_Immediate); } break;
                        INVALID_DEFAULT_CASE;
                }
            }
            
            current->memWrite.input = sel;
            if (asSibling)
            {
                current->sibling = prev->sibling;
                    prev->sibling = current;
                    result = prev;
            }
            else
            {
                if (useAluThru)
                {
                    // NOTE(michiel): We need to wait a cycle, so push this through the alu if possible
                    gen_ir_code_alu_thru(prog, prev, sel);
                    current->memWrite.input = Select_Alu;
                }
                buf_push(prog->codes, current);
            }
        } break;
        
        case IRCode_Calc:
        {
            
        } break;
        
        case IRCode_Immediate:
        {
                b32 asSibling = true;
                for (IRCode *entry = prev; entry; entry = entry->sibling)
                {
                    if ((entry->kind == IRCode_Immediate) ||
                        (entry->kind == IRCode_ReadB))
                    {
                        // NOTE(michiel): ReadB address overlaps immediate in op code so no sibling
                        asSibling = false;
                        break;
                    }
                }
                
                if (asSibling)
                {
                    current->sibling = prev->sibling;
                    prev->sibling = current;
                    result = prev;
                }
                else
                {
                    buf_push(prog->codes, current);
                }
        } break;
            
            INVALID_DEFAULT_CASE;
    }
    }
    else
    {
        // NOTE(michiel): No previous to check, so just push it on
        buf_push(prog->codes, current);
    }
    
    return result;
}

internal IRCode *
gen_ir_code_expr(IRProg *prog, Expr *expr, Expr *parent, IRCode *lastCode)
{
    IRCode *result = 0;
    
    switch (expr->kind)
    {
        case Expr_Paren:
        {
            result = gen_ir_code_expr(prog, expr->paren.expr, parent, lastCode);
        } break;
        
        case Expr_Int:
        {
            result = allocate_struct(IRCode, &prog->arena);
            result->kind = IRCode_Immediate;
            result->immediate = expr->intConst;
            result = gen_ir_code_combine(prog, lastCode, result);
        } break;
        
        case Expr_Id:
        {
            result = allocate_struct(IRCode, &prog->arena);
            result->kind = IRCode_ReadA;
            result->memRead.addrA = get_var_address(prog, expr->name);
            result = gen_ir_code_combine(prog, lastCode, result);
        } break;
        
        case Expr_Unary:
        {
            IRCode *operand = gen_ir_code_expr(prog, expr->unary.expr, expr, lastCode);
            switch (expr->unary.op)
            {
                case TOKEN_NOT:
                {
                    // NOTE(michiel): !a => a ? 0 : 1
                    fprintf(stderr, "'!' aka 'not' has not been implemented yet.\n");
                } break;
                
                case TOKEN_INV:
                {
                    // NOTE(michiel): ~a => a ^ -1
                } break;
                
                case TOKEN_NEG:
                {
                    // NOTE(michiel): -a => 0 - a
                } break;
                
                INVALID_DEFAULT_CASE;
            }

#if 0            
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
            #endif

        } break;
        
        case Expr_Binary:
        {
            IRCode *left  = gen_ir_code_expr(prog, expr->binary.left, expr, lastCode);
            IRCode *right = gen_ir_code_expr(prog, expr->binary.right, expr, left);
            
            if (parent)
            {
                
            }
            else
            {
                IRCode *alu = 
                result = right;
            }
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
gen_ir_code_assign(IRProg *prog, Expr *assignee, IRCode *exprCode)
{
    i_expect(assignee->kind == Expr_Id);
    String varName = assignee->name;
    
            if (strings_are_equal(varName, create_string("IO")))
    {
        IRCode *ioOut = allocate_struct(IRCode, &prog->buildArena);
        ioOut->kind = IRCode_IOOut;
        gen_ir_code_combine(prog, exprCode, ioOut);
    }
    else
    {
        u32 writeAddr = get_write_address(prog, varName);
        
        IRCode *memOut = allocate_struct(IRCode, &prog->buildArena);
        memOut->kind = IRCode_Write;
        memOut->memWrite.addr = writeAddr;
        gen_ir_code_combine(prog, exprCode, memOut);
        }
}

internal IRProg
generate_ir_code(AstOptimizer *optimizer)
{
    IRProg result = {0};
    result.regAllocMap = allocate_struct(Map, 0);
    
    for (u32 stmtIdx = 0; stmtIdx < optimizer->statements.stmtCount; ++stmtIdx)
    {
        Stmt *stmt = optimizer->statements.stmts[stmtIdx];
        if (stmt->kind == Stmt_Assign)
        {
            i_expect(stmt->assign.left->kind == Expr_Id);
             IRCode *exprCode = gen_ir_code_expr(&result, stmt->assign.right, 0);
            gen_ir_code_assign(&result, stmt->assign.left, exprCode);
        }
        else
        {
            i_expect(stmt->kind == Stmt_Hint);
            // TODO(michiel): Do something
            //u32 addr = gen_ir_code_expr(&result, stmt->expr, 0);
        }
    }
    
    return result;
}
#endif
