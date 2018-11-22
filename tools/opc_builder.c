internal inline u32
get_write_address(OpCodeBuilder *builder, String var)
{
    u32 result = builder->registerCount++;
    // NOTE(michiel): +1 to keep 0 as a error value.
    map_put_u64(&builder->registerMap, var.data, result + 1);
    return result;
}

internal inline u32
get_var_address(OpCodeBuilder *builder, String var)
{
    u64 result = map_get_u64(&builder->registerMap, var.data);
    i_expect(result);
    i_expect(result < U32_MAX);
    return (u32)(result - 1);
}

internal Selection
gen_opc_expr(OpCodeBuilder *builder, OpCodeEntry *assignee, Expr *expr)
{
     Selection result = 0;
    
    switch (expr->kind)
    {
        case Expr_Paren:
        {
            result = gen_opc_expr(builder, assignee, expr->paren.expr);
        } break;
        
        case Expr_Int:
        {
            result = Select_Immediate;
            assignee->useImmediate = true;
            assignee->immediate = safe_truncate_to_s32(expr->intConst);
        } break;
        
        case Expr_Id:
        {
            if (strings_are_equal(expr->name, create_string("IO")))
            {
                result = Select_IO;
            }
            else if (strings_are_equal(expr->name, create_string("ALU")))
            {
                result = Select_Alu;
            }
            else
            {
            u32 addr = get_var_address(builder, expr->name);
            if (assignee->useMemory)
                {
                if (assignee->memory.write)
                {
                    if (!assignee->memory.readA)
                    {
                        if (!assignee->memory.readB)
                        {
                            result = Select_MemoryB;
                            assignee->memory.readB = true;
                            assignee->memory.rAddrB = addr;
                        }
                        else if (addr == assignee->memory.wAddr)
                        {
                            result = assignee->memory.input;
                        }
                        else
                        {
                                OpCodeEntry nextRead = {0};
                                nextRead.memory.readA = true;
                                nextRead.memory.rAddrA = addr;
                                result = Select_MemoryA;
                                buf_push(builder->entries, nextRead);
                            //fprintf(stderr, "Cannot write and read twice at the same time!\n");
                            //INVALID_CODE_PATH;
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Ended up with a write and read at the same time!\n");
                        INVALID_CODE_PATH;
                    }
                }
                else if (assignee->memory.readA)
                {
                    if (!assignee->memory.readB)
                    {
                        result = Select_MemoryB;
                        assignee->memory.readB = true;
                        assignee->memory.rAddrB = addr;
                    }
                    else
                    {
                        fprintf(stderr, "Cannot read more than twice at the same time!\n");
                        INVALID_CODE_PATH;
                    }
                }
                else
                {
                    if (assignee->memory.readB)
                    {
                        result = Select_MemoryA;
                        assignee->memory.readA = true;
                        assignee->memory.rAddrA = addr;
                    }
                    else
                    {
                        fprintf(stderr, "Memory usage flag is set, but no memory is used!\n");
                        INVALID_CODE_PATH;
                    }
                }
            }
            else
            {
                result = Select_MemoryA;
                assignee->useMemory = true;
                assignee->memory.readA = true;
                assignee->memory.rAddrA = addr;
            }
            }
        } break;
        
        case Expr_Unary:
        {
            OpCodeEntry aluOpEntry = {0};
            aluOpEntry.useAlu = true;
            OpCodeEntry *alu = assignee;
            b32 pushBack = false;
            if (assignee->useAlu)
            {
                alu = &aluOpEntry;
                pushBack = true;
            }
                
            alu->useAlu = true;
            switch (expr->unary.op)
            {
                case TOKEN_NOT: {
                    fprintf(stderr, "'NOT'/'!' not implemented yet!\n");
                    INVALID_CODE_PATH;
                } break;
                case TOKEN_INV: {
                    alu->alu.inputA = Select_Immediate;
                    alu->alu.op = Alu_Xor;
                    alu->useImmediate = true;
                    alu->immediate = -1;
                } break;
                case TOKEN_NEG: {
                    alu->alu.op = Alu_Sub;
                } break;
                INVALID_DEFAULT_CASE;
            }
                alu->alu.inputB = gen_opc_expr(builder, alu, expr->unary.expr);
            
            if (pushBack)
            {
                buf_push(builder->entries, aluOpEntry);
            }
            
            result = Select_Alu;
            } break;
        
        case Expr_Binary:
        {
            OpCodeEntry aluOpEntry = {0};
            aluOpEntry.useAlu = true;
            OpCodeEntry *alu = assignee;
            b32 pushBack = false;
            if (assignee->useAlu)
            {
                pushBack = true;
                alu = &aluOpEntry;
            }
            
            alu->useAlu = true;
            switch (expr->binary.op)
            {
                case TOKEN_OR: { alu->alu.op = Alu_Or; } break;
                case TOKEN_XOR: { alu->alu.op = Alu_Xor; } break;
                case TOKEN_AND: { alu->alu.op = Alu_And; } break;
                case TOKEN_ADD: { alu->alu.op = Alu_Add; } break;
                case TOKEN_SUB: { alu->alu.op = Alu_Sub; } break;
                case TOKEN_MUL: { fprintf(stderr, "Multiply not yet supported!\n"); INVALID_CODE_PATH; } break;
                INVALID_DEFAULT_CASE;
            }
            alu->alu.inputA = gen_opc_expr(builder, alu, expr->binary.left);
            alu->alu.inputB = gen_opc_expr(builder, alu, expr->binary.right);
            
            if (pushBack)
            {
                buf_push(builder->entries, aluOpEntry);
            }
            
            result = Select_Alu;
        } break;
        
        INVALID_DEFAULT_CASE;
    }
    return result;
}

internal void
generate_opcodes(OpCodeBuilder *builder, AstOptimizer *optimizer)
{
    for (u32 stmtIdx = 0; stmtIdx < optimizer->statements.stmtCount; ++stmtIdx)
    {
        Stmt *stmt = optimizer->statements.stmts[stmtIdx];
        if (stmt->kind == Stmt_Assign)
        {
            i_expect(stmt->assign.left->kind == Expr_Id);
            
            OpCodeEntry entry = {0};
            Selection *output = 0;
            String varName = stmt->assign.left->name;
            if (strings_are_equal(varName, create_string("IO")))
            {
                entry.useIOOut = true;
                output = &entry.output.output;
            }
            else if (strings_are_equal(varName, create_string("ALU")))
            {
                entry.useAlu = true;
                output = &entry.alu.inputA;
            }
            else
            {
                entry.useMemory = true;
                entry.memory.write = true;
                entry.memory.wAddr = get_write_address(builder, varName);
                output = &entry.memory.input;
            }
            
            *output = gen_opc_expr(builder, &entry, stmt->assign.right);
            
            buf_push(builder->entries, entry);
        }
        else
        {
            i_expect(stmt->kind == Stmt_Hint);
            // TODO(michiel): 
        }
    }
}

internal void
flush_opcode(OpCode **buffer, OpCode *opc)
{
    buf_push(*buffer, *opc);
    opc->selectAluA = Select_Zero;
    opc->selectAluB = Select_Zero;
    opc->selectMem = Select_Zero;
    opc->selectIO = Select_Zero;
    opc->aluOperation = Alu_Noop;
    opc->memoryReadA = false;
    opc->memoryReadB = false;
    opc->memoryWrite = false;
    opc->immediate = 0;
    opc->memoryAddrA = 0;
    opc->memoryAddrB = 0;
}

internal OpCode *
layout_instructions(OpCodeBuilder *builder)
{
    OpCode *result = 0;
    
    OpCode current = {0};
    for (u32 entryIdx = 0; entryIdx < buf_len(builder->entries); ++entryIdx)
    {
        // NOTE(michiel): These entries do not know about timing, so everything is
        // packed in one frame. We decompose and see what we can reuse.
        OpCodeEntry *entry = builder->entries + entryIdx;
        
        Selection replaceMemA = Select_Zero;
        Selection replaceMemB = Select_Zero;
        
        if (entry->useMemory && (entry->memory.readA || entry->memory.readB))
        {
            // NOTE(michiel): Read request, see if there is still space for a
            // read, otherwise we have a double flush
            if (!(current.memoryReadA || current.memoryReadB || current.memoryWrite))
            {
                // NOTE(michiel): Simple case, prev does not yet use memory
                if (entry->memory.readA)
                {
                current.memoryReadA = true;
                    current.memoryAddrA = entry->memory.rAddrA;
                }
                if (entry->memory.readB)
                {
                    current.memoryReadB = true;
                    current.memoryAddrB = entry->memory.rAddrB;
                }
                if (entry->useAlu && 
                    (current.aluOperation == Alu_Noop) &&
                    (current.selectAluA == Select_Zero))
                {
                    current.selectAluA = Select_Alu;
                }
            }
            else if (current.memoryWrite)
            {
                if (entry->memory.readA)
                {
                if ((entry->memory.rAddrA == current.memoryAddrA) &&
                         (current.aluOperation == Alu_Noop) &&
                            (current.selectAluA == Select_Zero))
                    {
                             current.selectAluA = current.selectMem;
                        replaceMemA = Select_Alu;
                    }
                    else if (!current.memoryReadB && !entry->memory.readB)
                    {
                        current.memoryReadB = true;
                        current.memoryAddrB = entry->memory.rAddrA;
                        replaceMemA = Select_MemoryB;
                    }
                    else
                    {
                        if (entry->useAlu && 
                            (current.aluOperation == Alu_Noop) &&
                            (current.selectAluA == Select_Zero))
                        {
                            current.selectAluA = Select_Alu;
                            replaceMemA = Select_Alu;
                        }
                        else
                        {
                        flush_opcode(&result, &current);
                        }
                        current.memoryReadA = true;
                        current.memoryAddrA = entry->memory.rAddrA;
                    }
                }
                if (entry->memory.readB)
                {
                    if ((entry->memory.rAddrB == current.memoryAddrA) &&
                        (current.aluOperation == Alu_Noop) &&
                        (current.selectAluA == Select_Zero))
                    {
                        current.selectAluA = current.selectMem;
                        replaceMemB = Select_Alu;
                    }
                    else
                    {
                        if (entry->useAlu && 
                            (current.aluOperation == Alu_Noop) &&
                            (current.selectAluA == Select_Zero))
                        {
                            current.selectAluA = Select_Alu;
                            replaceMemB = Select_Alu;
                        }
                        
                        if (current.memoryReadB)
                        {
                            flush_opcode(&result, &current);
                        }
                        current.memoryReadB = true;
                        current.memoryAddrB = entry->memory.readB;
                        }
                }
            }
            else if (current.memoryReadA)
            {
                if (entry->memory.readA ||
                    (entry->memory.readB && current.memoryReadB))
                {
                    if (entry->useAlu && 
                        (current.aluOperation == Alu_Noop) &&
                        (current.selectAluA == Select_Zero))
                    {
                        current.selectAluA = Select_Alu;
                    }
                    flush_opcode(&result, &current);
                }
                if (entry->memory.readA)
                {
                current.memoryReadA = true;
                current.memoryAddrA = entry->memory.rAddrA;
                }
                if (entry->memory.readB)
                {
                    current.memoryReadB = true;
                    current.memoryAddrB = entry->memory.rAddrB;
                }
            }
            else
            {
                i_expect(current.memoryReadB);
                if (entry->memory.readB)
                {
                    if (entry->useAlu && 
                        (current.aluOperation == Alu_Noop) &&
                        (current.selectAluA == Select_Zero))
                    {
                        current.selectAluA = Select_Alu;
                    }
                    flush_opcode(&result, &current);
                }
                if (entry->memory.readA)
                {
                    current.memoryReadA = true;
                    current.memoryAddrA = entry->memory.rAddrA;
                }
                if (entry->memory.readB)
                {
                    current.memoryReadB = true;
                    current.memoryAddrB = entry->memory.rAddrB;
                }
            }
            }
        
        if (entry->useAlu)
        {
            if (entry->useMemory && 
                (entry->memory.readA || entry->memory.readB) &&
                ((entry->alu.inputA == Select_MemoryA) ||
                 (entry->alu.inputA == Select_MemoryB) ||
                (entry->alu.inputB == Select_MemoryA) ||
                 (entry->alu.inputB == Select_MemoryB)))
            {
                flush_opcode(&result, &current);
            }
            
            if ((current.aluOperation != Alu_Noop) ||
                (current.selectAluA != Select_Zero))
            {
                flush_opcode(&result, &current);
            }
            
            if (entry->useImmediate)
            {
                if ((current.selectMem == Select_Immediate) ||
                    (current.selectIO == Select_Immediate))
                {
                    if ((current.aluOperation == Alu_Noop) &&
                        (current.selectAluA == Select_Zero))
                    {
                        current.selectAluA = Select_Alu;
                    }
                    flush_opcode(&result, &current);
                }
                current.immediate = entry->immediate;
            }
            
            current.aluOperation = entry->alu.op;
            
            if ((replaceMemA != Select_Zero) && (entry->alu.inputA == Select_MemoryA))
            {
                current.selectAluA = replaceMemA;
            }
            else if ((replaceMemB != Select_Zero) && (entry->alu.inputA == Select_MemoryB))
            {
                current.selectAluA = replaceMemB;
            }
            else
            {
                current.selectAluA = entry->alu.inputA;
            }
            
            if ((replaceMemA != Select_Zero) && (entry->alu.inputB == Select_MemoryA))
            {
                current.selectAluB = replaceMemA;
            }
            else if ((replaceMemB != Select_Zero) && (entry->alu.inputB == Select_MemoryB))
            {
                current.selectAluB = replaceMemB;
            }
            else
            {
                current.selectAluB = entry->alu.inputB;
            }
            }
        
        if (entry->useIOOut)
        {
            if ((entry->useAlu && (entry->output.output == Select_Alu)) ||
                (entry->useMemory && (entry->memory.readA || entry->memory.readB) &&
                ((entry->output.output == Select_MemoryA) ||
                  (entry->output.output == Select_MemoryB))))
            {
                flush_opcode(&result, &current);
            }
            
            if (current.selectIO != Select_Zero)
            {
                if ((entry->output.output == Select_Alu) &&
                    (current.aluOperation == Alu_Noop) &&
                    (current.selectAluA == Select_Zero))
                {
                    current.selectAluA = Select_Alu;
                }
                flush_opcode(&result, &current);
            }
            if (entry->output.output == Select_Immediate)
            {
                current.immediate = entry->immediate;
            }
            
            if ((replaceMemA != Select_Zero) && (entry->output.output == Select_MemoryA))
            {
                current.selectIO = replaceMemA;
            }
            else if ((replaceMemB != Select_Zero) && (entry->output.output == Select_MemoryB))
            {
                current.selectIO = replaceMemB;
            }
            else
            {
                current.selectIO = entry->output.output;
            }
        }
        
        if (entry->useMemory && entry->memory.write)
        {
            if ((entry->useAlu && (entry->memory.input == Select_Alu)) ||
                (entry->useMemory && (entry->memory.readA || entry->memory.readB) &&
                 ((entry->memory.input == Select_MemoryA) ||
                  (entry->memory.input == Select_MemoryB))))
            {
                flush_opcode(&result, &current);
            }

            if (current.memoryReadA || current.memoryWrite)
            {
                if ((entry->memory.input == Select_Alu) &&
                    (current.aluOperation == Alu_Noop) &&
                    (current.selectAluA == Select_Zero))
                {
                    current.selectAluA = Select_Alu;
                }
                flush_opcode(&result, &current);
            }
            
            if (entry->memory.input == Select_Immediate)
            {
                current.immediate = entry->immediate;
            }
            current.memoryWrite = true;
            current.memoryAddrA = entry->memory.wAddr;
            
            if ((replaceMemA != Select_Zero) && (entry->memory.input == Select_MemoryA))
            {
                current.selectMem = replaceMemA;
            }
            else if ((replaceMemB != Select_Zero) && (entry->memory.input == Select_MemoryB))
            {
                current.selectMem = replaceMemB;
            }
            else
            {
                current.selectMem = entry->memory.input;
            }
        }
    }
    
    flush_opcode(&result, &current);
    
    return result;
}

internal void
print_opcodes(u32 opcCount, OpCodeEntry *entries)
{
    for (u32 entryIdx = 0; entryIdx < opcCount; ++entryIdx)
    {
        OpCodeEntry *entry = entries + entryIdx;

#if 1
        String memInA = {0};
        String memInB = {0};
        if (entry->useMemory && (entry->memory.readA || entry->memory.readB))
        {
            if (entry->memory.readA)
            {
                memInA = create_string_fmt("%s = mem[%d]", gSelectionNames[Select_MemoryA],
                                           entry->memory.rAddrA);
            }
            if (entry->memory.readB)
            {
                memInB = create_string_fmt("%s = mem[%d]", gSelectionNames[Select_MemoryB],
                                           entry->memory.rAddrB);
            }
        }
        String imm = {0};
        if (entry->useImmediate)
        {
            imm = create_string_fmt("%s = %d", gSelectionNames[Select_Immediate],
                                    entry->immediate);
        }
        String alu = {0};
        if (entry->useAlu)
        {
            alu = create_string_fmt("%s = %s %s %s", gSelectionNames[Select_Alu],
                                    gSelectionNames[entry->alu.inputA],
                                    gAluOpNames[entry->alu.op],
                                    gSelectionNames[entry->alu.inputB]);
        }
        String writeMem = {0};
        if (entry->useMemory && entry->memory.write)
        {
            writeMem = create_string_fmt("mem[%d] = %s", entry->memory.wAddr,
                                         gSelectionNames[entry->memory.input]);
        }
        String ioOut = {0};
        if (entry->useIOOut)
        {
            ioOut = create_string_fmt("IO = %s", gSelectionNames[entry->output.output]);
        }
        
        if (memInA.size || memInB.size)
        {
            fprintf(stdout, "Read: %.*s|%.*s || ", 
                    memInA.size, memInA.data, memInB.size, memInB.data);
        }
        if (imm.size)
        {
            fprintf(stdout, "Imm: %.*s || ", imm.size, imm.data);
        }
        if (alu.size)
        {
            fprintf(stdout, "Exec: %.*s || ", alu.size, alu.data);
        }
        if (writeMem.size)
        {
            fprintf(stdout, "Write: %.*s || ", writeMem.size, writeMem.data);
        }
        if (ioOut.size)
        {
            fprintf(stdout, "Out: %.*s || ", ioOut.size, ioOut.data);
        }
        fprintf(stdout, "\n");
        #else
        fprintf(stdout, "Usage: %s%s%s%s%s\n", 
                entry->useMemory ? "Mem " : "", 
                entry->useAlu ? "Alu " : "", 
                entry->useIOIn ? "In " : "", 
                entry->useIOOut ? "Out " : "", 
                entry->useImmediate ? "Imm" : "");
        if (entry->useMemory)
        {
            fprintf(stdout, "  Mem: %s%s%s\n",
                    entry->memory.write ? "w " : "",
                    entry->memory.readA ? "rA " : "",
                    entry->memory.readB ? "rB " : "");
            if (entry->memory.write)
            {
                fprintf(stdout, "    Wr: %s @ %d\n", gSelectionNames[entry->memory.input],
                        entry->memory.wAddr);
            }
            if (entry->memory.readA)
            {
                fprintf(stdout, "    RA: @ %d\n", entry->memory.rAddrA);
            }
            if (entry->memory.readB)
            {
                fprintf(stdout, "    RB: @ %d\n", entry->memory.rAddrB);
            }
        }
        if (entry->useAlu)
        {
            fprintf(stdout, "  Alu: %s %s %s\n", gSelectionNames[entry->alu.inputA],
                    gAluOpNames[entry->alu.op], gSelectionNames[entry->alu.inputB]);
        }
        if (entry->useIOOut)
        {
            fprintf(stdout, "  Out: %s\n", gSelectionNames[entry->output.output]);
        }
        if (entry->useImmediate)
        {
            fprintf(stdout, "  Imm: %d\n", entry->immediate);
        }
        #endif

    }
}
