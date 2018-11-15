internal void
opc_optimize_unused_alu(u32 opcCount, OpCode *opc)
{
    // NOTE(michiel): Resets the input select a/b from the previous opcode to 0
    // iff the current opcode doesn't use the alu.
    
    for (s32 opIdx = opcCount - 1; opIdx > 0; --opIdx)
    {
        OpCode *cur = opc + opIdx;
        OpCode *prev = opc + opIdx - 1;
        
        if (opIdx == (opcCount - 1))
        {
            cur->selectAluA = Select_Zero;
            cur->selectAluB = Select_Zero;
        }
        
        if (!opc_has_select(cur, Select_Alu))
        {
            prev->selectAluA = Select_Zero;
            prev->selectAluB = Select_Zero;
        }
    }
}

internal b32
opc_uses_mem(OpCode *opc)
{
    b32 result = false;
    if (opc_has_select(opc, Select_MemoryA) ||
        opc_has_select(opc, Select_MemoryB) ||
        opc->memoryWrite ||
        opc->memoryReadA ||
        opc->memoryReadB)
    {
        result = true;
    }
    return result;
}

internal b32
opc_uses_alu(OpCode *opc)
{
    b32 result = false;
    if (opc_has_select(opc, Select_Alu) ||
        (opc->selectAluA != Select_Zero) ||
        (opc->selectAluB != Select_Zero) ||
        (opc->aluOperation != Alu_Noop))
    {
        result = true;
    }
    return result;
}

internal b32
opc_uses_io_out(OpCode *opc)
{
    b32 result = (opc->selectIO != Select_Zero);
    return result;
}

internal b32
opc_uses_io_in(OpCode *opc)
{
    b32 result = opc_has_select(opc, Select_IO);
    return result;
}

internal b32
opc_uses_immediate(OpCode *opc)
{
    b32 result = opc_has_select(opc, Select_Immediate);
    return result;
}

internal void
opc_switch_mem_a_to_b(OpCode *opc)
{
    if (opc->selectAluA == Select_MemoryA)
    {
        opc->selectAluA = Select_MemoryB;
    }
    if (opc->selectAluB == Select_MemoryA)
    {
        opc->selectAluB = Select_MemoryB;
    }
    if (opc->selectMem == Select_MemoryA)
    {
        opc->selectMem = Select_MemoryB;
    }
    if (opc->selectIO == Select_MemoryA)
    {
        opc->selectIO = Select_MemoryB;
    }
}

internal void
opc_switch_mem_b_to_a(OpCode *opc)
{
    if (opc->selectAluA == Select_MemoryB)
    {
        opc->selectAluA = Select_MemoryA;
    }
    if (opc->selectAluB == Select_MemoryB)
    {
        opc->selectAluB = Select_MemoryA;
    }
    if (opc->selectMem == Select_MemoryB)
    {
        opc->selectMem = Select_MemoryA;
    }
    if (opc->selectIO == Select_MemoryB)
    {
        opc->selectIO = Select_MemoryA;
    }
}

internal b32
opc_try_merge(OpCode *next, OpCode *current, OpCode *nextNext)
{
    // NOTE(michiel): nextNext is used to patch selects in case of mem swapping
    b32 result = false;
    
    if (!(opc_uses_mem(next) ||
          opc_uses_alu(next) ||
          opc_uses_io_out(next) ||
          opc_uses_io_in(next) ||
          opc_uses_immediate(next)))
    {
        // NOTE(michiel): Empty next
        result = true;
    }
    else if (opc_uses_io_out(next) && opc_uses_io_out(current))
    {
        // NOTE(michiel): Both instructions wanna output data to the IO, so not mergeable.
    }
    else if (next->memoryWrite && current->memoryWrite)
    {
        // NOTE(michiel): Both instructions wanna write to memory, so not mergeable.
    }
    else if (opc_uses_alu(next) && opc_uses_alu(current))
    {
        // NOTE(michiel): Both instructions use the alu, so probably not mergeable.
        // TODO(michiel): Find out for sure!
    }
    else
    {
        if ((next->selectAluA == Select_Zero) &&
            (next->selectAluB == Select_Zero) &&
            (next->selectIO == Select_Zero) &&
            (next->selectMem == Select_Zero) &&
            ((next->memoryReadA && !next->memoryReadB) ||
             (!next->memoryReadA && next->memoryReadB)))
        {
            i_expect(!next->memoryWrite); // NOTE(michiel): Implied by select zero
            
            if (!current->memoryWrite && !current->memoryReadA)
            {
                if (next->memoryReadA)
                {
                    current->memoryReadA = true;
                    current->memoryAddrA = next->memoryAddrA;
                    result = true;
                }
                else
                {
                    if (nextNext)
                    {
                        i_expect(next->memoryReadB);
                    current->memoryReadA = true;
                current->memoryAddrA = next->memoryAddrB;
                        result = true;
                        opc_switch_mem_b_to_a(nextNext);
                    }
                    }
            }
            if (!result && !current->memoryReadB &&
                !(current->memoryWrite && ((current->memoryAddrA == next->memoryAddrA) ||
                                           (current->memoryAddrA == next->memoryAddrB))))
            {
                if (next->memoryReadB)
                {
                current->memoryReadB = true;
                    current->memoryAddrB = next->memoryAddrB;
                    result = true;
                }
                else
                {
                    if (nextNext)
                    {
                        i_expect(next->memoryReadA);
                        current->memoryReadB = true;
                        current->memoryAddrB = next->memoryAddrA;
                        result = true;
                        opc_switch_mem_a_to_b(nextNext);
                    }
                }
            }
        }
    }
    
    return result;
}

internal void
opc_optimize_mergers(u32 opcCount, OpCode *opc)
{
    // NOTE(michiel): Resets the input select a/b from the previous opcode to 0
    // iff the current opcode doesn't use the alu.
    
    for (s32 opIdx = opcCount - 1; opIdx > 0; --opIdx)
    {
        OpCode *cur = opc + opIdx;
        OpCode *prev = opc + opIdx - 1;
        OpCode *next = 0;
        
        if (opIdx < (opcCount - 1))
        {
            next = opc + opIdx + 1;
        }
    
        b32 merged = opc_try_merge(cur, prev, next);
        if (merged)
        {
            for (u32 popIdx = opIdx; popIdx < (opcCount - 1); ++popIdx)
            {
                opc[popIdx] = opc[popIdx + 1];
            }
            --buf_len_(opc);
        }
    }
}

#if 0
internal void
opc_optimize_move_read_forward(u32 opcCount, OpCode *opc)
{
    for (s32 opIdx = opcCount - 1; opIdx > 0; --opIdx)
    {
        OpCode *cur = opc + opIdx;
        OpCode *prev = opc + opIdx - 1;
        
        if (opIdx == (opcCount - 1))
        {
            cur->memoryReadA = false;
            if (!cur->memoryWrite)
            {
                cur->memoryAddrA = 0;
            }
            cur->memoryReadB = false;
        cur->memoryAddrB = 0;
            }
        
        //
        // NOTE(michiel): Clean up single b reads
        //
        if (cur->memoryReadB && !cur->memoryReadA && !cur->memoryWrite)
        {
            cur->memoryReadA = true;
            cur->memoryAddrA = cur->memoryAddrB;
            cur->memoryReadB = false;
            cur->memoryAddrB = 0;
        }
        
        if (opc_has_select(cur, Select_MemoryA))
        {
        }
        
        if (!opc_has_select(cur, Select_Alu))
        {
            prev->selectAluA = Select_Zero;
            prev->selectAluB = Select_Zero;
        }
    }
}
#endif

#if 0
// NOTE(michiel): Old code

internal void
optimize_assignments(OpCode **opCodes)
{
    // NOTE(michiel): Removes alu no-ops to assignment selection
    u32 opCodeCount = buf_len(*opCodes);
    for (s32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCode cur = (*opCodes)[opIdx];
        OpCode prev = (*opCodes)[opIdx - 1];
        
        if (opc_only_selection(&cur))
        {
            // NOTE(michiel): Only selection is set for cur
            if (opc_only_selection(&prev) &&
                prev.selectAluA && !prev.selectAluB &&
                !prev.selectMem && !prev.selectIO)
            {
                // NOTE(michiel): Only alu A selection with a alu no-op for prev
                
#if 0                
                fprintf(stdout, "Changing assignments around:\n");
                fprintf(stdout, "Prev:\n");
                print_opcode_single(&prev);
                fprintf(stdout, "Cur:\n");
                print_opcode_single(&cur);
#endif
                
                // TODO(michiel): Is this correct??
                if (cur.selectAluA)
                {
                    // NOTE(michiel): Nothing to do
                }
                if (cur.selectAluB)
                {
                    prev.selectAluB = prev.selectAluA;
                }
                if (cur.selectMem)
                {
                    prev.selectMem = prev.selectAluA;
                }
                if (cur.selectIO)
                {
                    prev.selectIO = prev.selectAluA;
                }
                
#if 0                
                fprintf(stdout, "Replace:\n");
                print_opcode_single(&prev);
                fprintf(stdout, "----------------------------\n");
#endif
                
                for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                {
                    (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                }
                --buf_len_(*opCodes);
                --opCodeCount;
                --opIdx;
                (*opCodes)[opIdx] = prev;
            }
        }
    }
}

internal void
optimize_read_setups(OpCode **opCodes)
{
    // NOTE(michiel): Combines read setup with previous instructions that don't do
    // memory manipulation or only do a write to the same address.
    // It's a little more involved cause it also depends on the next instructions use
    // of the read.
    u32 opCodeCount = buf_len(*opCodes);
    for (s32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCode cur = (*opCodes)[opIdx];
        OpCode prev = (*opCodes)[opIdx - 1];
        
        if ((cur.memoryReadA || cur.memoryReadB) && !cur.memoryWrite &&
            ((!cur.selectAluA && !cur.selectAluB && !cur.selectMem && !cur.selectIO) ||
             ((cur.selectAluA == Select_Alu) &&
              !cur.selectAluB && !cur.selectMem && !cur.selectIO &&
              (cur.aluOperation == Alu_Noop))))
        {
            // NOTE(michiel): Read setup for mem A or mem B
            
            if (!prev.memoryReadA && !prev.memoryReadB && !prev.memoryWrite)
            {
                // NOTE(michiel): If previous doesn't use memory
                if (cur.memoryReadA)
                {
                    prev.memoryReadA = cur.memoryReadA;
                    prev.memoryAddrA = cur.memoryAddrA;
                }
                if (cur.memoryReadB)
                {
                    prev.memoryReadB = cur.memoryReadB;
                    prev.memoryAddrB = cur.memoryAddrB;
                }
                
                for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                {
                    (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                }
                --buf_len_(*opCodes);
                --opCodeCount;
                --opIdx;
                (*opCodes)[opIdx] = prev;
            }
            
            if (prev.memoryWrite && !prev.memoryReadA && !prev.memoryReadB &&
                ((prev.memoryAddrA == cur.memoryAddrA) ||
                 (prev.memoryAddrA == cur.memoryAddrB)))
            {
                // NOTE(michiel): Previous was write to the same memory address
                
                if (opIdx < (opCodeCount - 1))
                {
                    OpCode next = (*opCodes)[opIdx + 1];
                    
                    b32 changed = false;
                    if ((prev.memoryAddrA == cur.memoryAddrA) && ((next.selectAluA == Select_MemoryA) ||
                                                                  (next.selectAluB == Select_MemoryA) ||
                                                                  (next.selectMem == Select_MemoryA) ||
                                                                  (next.selectIO == Select_MemoryA)))
                    {
                        if (next.selectAluA == Select_MemoryA)
                        {
                            next.selectAluA = Select_Alu;
                        }
                        if (next.selectAluB == Select_MemoryA)
                        {
                            next.selectAluB = Select_Alu;
                        }
                        if (next.selectMem == Select_MemoryA)
                        {
                            next.selectMem = Select_Alu;
                        }
                        if (next.selectIO == Select_MemoryA)
                        {
                            next.selectIO = Select_Alu;
                        }
                        
                        prev.selectAluA = Select_Alu;
                        changed = true;
                    }
                    
                    if ((prev.memoryAddrA == cur.memoryAddrB) &&
                        ((next.selectAluA == Select_MemoryB) ||
                         (next.selectAluB == Select_MemoryB) ||
                         (next.selectMem == Select_MemoryB) ||
                         (next.selectIO == Select_MemoryB)))
                    {
                        if (next.selectAluA == Select_MemoryB)
                        {
                            next.selectAluA = Select_Alu;
                        }
                        if (next.selectAluB == Select_MemoryB)
                        {
                            next.selectAluB = Select_Alu;
                        }
                        if (next.selectMem == Select_MemoryB)
                        {
                            next.selectMem = Select_Alu;
                        }
                        if (next.selectIO == Select_MemoryB)
                        {
                            next.selectIO = Select_Alu;
                        }
                        
                        prev.selectAluA = Select_Alu;
                        changed = true;
                    }
                    
                    if (changed)
                    {
                        
                        for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                        {
                            (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                        }
                        --buf_len_(*opCodes);
                        --opCodeCount;
                        (*opCodes)[opIdx--] = next;
                        (*opCodes)[opIdx] = prev;
                    }
                }
            }
        }
    }
}

internal void
optimize_combine_write(OpCode **opCodes)
{
    // NOTE(michiel): Combines a alu operation on a previous output _if_ the
    // previous output was generated by a alu no-op.
    u32 opCodeCount = buf_len(*opCodes);
    for (s32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCode cur = (*opCodes)[opIdx];
        OpCode prev = (*opCodes)[opIdx - 1];
        
        if (cur.aluOperation &&
            ((cur.selectAluA == Select_Alu) ||
             (cur.selectAluB == Select_Alu)) &&
            ((cur.selectAluA == Select_Immediate) ||
             (cur.selectAluB == Select_Immediate)) &&
            !(cur.memoryReadA || cur.memoryReadB || cur.memoryWrite))
        {
            // NOTE(michiel): Alu operation with alu out as one of the inputs and no memory mods
            
            if ((prev.aluOperation == Alu_Noop) &&
                !prev.memoryReadA && !prev.memoryReadB)
            {
                prev.aluOperation = cur.aluOperation;
                
                if (cur.selectAluA == Select_Alu)
                {
                    prev.selectAluB = cur.selectAluB;
                }
                else
                {
                    prev.selectAluB = cur.selectAluA;
                }
                prev.immediate = cur.immediate;
                
                if (cur.selectIO)
                {
                    prev.selectIO = cur.selectIO;
                }
                
                for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                {
                    (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                }
                --buf_len_(*opCodes);
                --opCodeCount;
                --opIdx;
                (*opCodes)[opIdx] = prev;
            }
        }
    }
}

internal void
optimize_alu_noop(OpCode **opCodes)
{
    // NOTE(michiel): Removes alu no-op and connects the switches directly to it's alu a connection
    u32 opCodeCount = buf_len(*opCodes);
    
    for (s32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCode *cur = (*opCodes) + opIdx;
        OpCode *prev = (*opCodes) + opIdx - 1;
        b32 update = false;
        
        if ((prev->aluOperation == Alu_Noop) && 
            prev->selectAluA)
        {
            if (opc_only_selection(prev))
            {
                if (cur->selectAluA == Select_Alu)
                {
                    cur->selectAluA = prev->selectAluA;
                }
                if (cur->selectAluB == Select_Alu)
                {
                    cur->selectAluB = prev->selectAluA;
                }
                if (cur->selectMem == Select_Alu)
                {
                    cur->selectMem = prev->selectAluA;
                }
                if (cur->selectIO == Select_Alu)
                {
                    cur->selectIO = prev->selectAluA;
                }
                
                if (prev->selectIO && !cur->selectIO)
                {
                    cur->selectIO = prev->selectIO;
                }
                
                *prev = *cur;
                update = true;
            }
            
            if (prev->memoryWrite && !prev->memoryReadA && !prev->memoryReadB &&
                !cur->memoryWrite && !cur->memoryReadA && !cur->memoryReadB)
            {
                if (cur->selectAluA == Select_Alu)
                {
                    cur->selectAluA = prev->selectAluA;
                }
                if (cur->selectAluB == Select_Alu)
                {
                    cur->selectAluB = prev->selectAluA;
                }
                if (cur->selectMem == Select_Alu)
                {
                    // NOTE(michiel): Do nothing, it is not marked as a write, so this select 
                    // isn't used.
                    //INVALID_CODE_PATH;
                    //cur->selectMem = prev->selectAluA;
                }
                if (cur->selectIO == Select_Alu)
                {
                    cur->selectIO = prev->selectAluA;
                }
                
                if (prev->selectIO && !cur->selectIO)
                {
                    cur->selectIO = prev->selectIO;
                }
                
                cur->selectMem = prev->selectMem;
                cur->memoryWrite = prev->memoryWrite;
                cur->memoryAddrA = prev->memoryAddrA;
                
                *prev = *cur;
                update = true;
            }
            
            if (((prev->selectAluA == prev->selectMem) ||
                 (prev->selectAluA == Select_Zero)) &&
                (prev->memoryWrite && prev->memoryReadB &&
                 (prev->memoryAddrA == prev->memoryAddrB)))
            {
                prev->selectAluA = prev->selectMem;
                prev->memoryReadB = false;
                prev->memoryAddrB = 0;
                if (cur->selectAluA == Select_MemoryB)
                {
                    cur->selectAluA = Select_Alu;
                }
                if (cur->selectAluB == Select_MemoryB)
                {
                    cur->selectAluB = Select_Alu;
                }
                if (cur->selectMem == Select_MemoryB)
                {
                    cur->selectMem = Select_Alu;
                }
                if (cur->selectIO == Select_MemoryB)
                {
                    cur->selectIO = Select_Alu;
                }
            }
            
            if (!opc_has_select(cur, Select_Alu))
            {
                // NOTE(michiel): Alu not used
                //prev->selectAluA = Select_Zero;
                //prev->selectAluB = Select_Zero;
            }
            
            if (update)
            {
                for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                {
                    (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                }
                --buf_len_(*opCodes);
                --opCodeCount;
                --opIdx;
            }
        }
    }
}

internal void
optimize_combine_read(OpCode **opCodes)
{
    // NOTE(michiel): If current is a single mem read and prev is a single mem write, combine them
    // Also moves a single read from B to port A for further optimizations
    
    u32 opCodeCount = buf_len(*opCodes);
    
    for (s32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCode *cur = (*opCodes) + opIdx;
        OpCode *prev = (*opCodes) + opIdx - 1;
        
        if (!cur->memoryWrite && !cur->memoryReadA && cur->memoryReadB)
        {
            cur->memoryReadA = cur->memoryReadB;
            cur->memoryReadB = false;
            cur->memoryAddrA = cur->memoryAddrB;
            cur->memoryAddrB = 0;
            
            i_expect(opIdx < opCodeCount - 1);
            OpCode *next = cur + 1;
            if (next->selectAluA == Select_MemoryB)
            {
                next->selectAluA = Select_MemoryA;
            }
            if (next->selectAluB == Select_MemoryB)
            {
                next->selectAluB = Select_MemoryA;
            }
            if (next->selectMem == Select_MemoryB)
            {
                next->selectMem = Select_MemoryA;
            }
            if (next->selectIO == Select_MemoryB)
            {
                next->selectIO = Select_MemoryA;
            }
        }
        
        if (prev->memoryWrite && !prev->memoryReadA && !prev->memoryReadB &&
            !cur->memoryWrite && cur->memoryReadA && !cur->memoryReadB &&
            !prev->aluOperation && !cur->aluOperation)
        {
            // NOTE(michiel): Previous was write, cur is read
            prev->memoryReadB = true;
            prev->memoryAddrB = cur->memoryAddrA;
            
            for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
            {
                (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
            }
            --buf_len_(*opCodes);
            --opCodeCount;
            --opIdx;
            
            if (cur->selectAluA == Select_MemoryA)
            {
                cur->selectAluA = Select_MemoryB;
            }
            if (cur->selectAluB == Select_MemoryA)
            {
                cur->selectAluB = Select_MemoryB;
            }
            if (cur->selectMem == Select_MemoryA)
            {
                cur->selectMem = Select_MemoryB;
            }
            if (cur->selectIO == Select_MemoryA)
            {
                cur->selectIO = Select_MemoryB;
            }
        }
    }
}
#endif
