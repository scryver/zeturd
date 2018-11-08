#include "./common.h"
#include "./tokenizer.h"
#include "./parser.h"

#include "./common.c"
#include "./tokenizer.c"
#include "./parser.c"

#define REG_MAX (1 << 9)

enum Selection
{
    Select_Zero,
    Select_MemoryA,
    Select_MemoryB,
    Select_Immediate,
    Select_IO,
    Select_Alu,
    
    Select_Count,
};

enum AluOp
{
    Alu_Noop,
    Alu_And,
    Alu_Or,
    Alu_Add,
    
    Alu_Count,
};

typedef struct OpCodeBuild
{
    enum Selection selectAluA;
    enum Selection selectAluB;
    enum Selection selectMem;
    enum Selection selectIO;
    
    enum AluOp aluOperation;
    
    b32 memoryReadA;
    b32 memoryReadB;
    b32 memoryWrite;
    
    b32 useB;
u32 immediate;
    u32 memoryAddrA;
    u32 memoryAddrB;
    } OpCodeBuild;

internal b32 opc_only_selection(OpCodeBuild *opCode)
{
    b32 result = false;
    if ((opCode->selectAluA || opCode->selectAluB ||
         opCode->selectMem || opCode->selectIO) &&
        !(opCode->aluOperation || opCode->memoryReadA ||
          opCode->memoryReadB || opCode->memoryWrite))
    {
        result = true;
    }
    return result;
}

internal b32 opc_has_data(OpCodeBuild *opCode)
{
    b32 result = false;
    if (opCode->selectAluA || opCode->selectAluB ||
        opCode->selectMem || opCode->selectIO ||
        opCode->aluOperation ||
        opCode->memoryReadA || opCode->memoryReadB ||
        opCode->memoryWrite)
    {
        result = true;
    }
    return result;
}

internal u32 opc_max_immediate(u32 opCount, OpCodeBuild *opCodes)
{
    u32 maxImmediate = 0;
    for (u32 opIdx = 0; opIdx < opCount; ++opIdx)
    {
        OpCodeBuild opCode = opCodes[opIdx];
        if (maxImmediate < opCode.immediate)
        {
            maxImmediate = opCode.immediate;
        }
    }
    return maxImmediate;
}

internal u32 opc_max_address(u32 opCount, OpCodeBuild *opCodes)
{
    u32 maxAddress = 0;
    for (u32 opIdx = 0; opIdx < opCount; ++opIdx)
    {
        OpCodeBuild opCode = opCodes[opIdx];
        if (maxAddress < opCode.memoryAddrA)
        {
            maxAddress = opCode.memoryAddrA;
        }
        if (maxAddress < opCode.memoryAddrB)
        {
            maxAddress = opCode.memoryAddrB;
        }
    }
    return maxAddress;
}

typedef struct OpCodeStats
{
    u32 bitWidth;
    
    u32 maxSelect;
    u32 selectBits;
    u32 maxAluOp;
    u32 aluOpBits;
    u32 maxImmediate;
    u32 immediateBits;
    u32 maxAddress;
    u32 addressBits;
    
    u32 opCodeCount;
    u32 opCodeBits;
    
    u32 opCodeBitWidth;
} OpCodeStats;

#pragma pack(push, 1)
typedef union OpCode
{
    struct
    {
        union
        {
            struct 
            {
                u64 padding     : 23;
                u64 memoryAddrB : 9;
            };
            u64 immediate   : 32;
        };
        u64 memoryAddrA  : 9;
        u64 aluOperation : 7;
        u64 useMemoryB   : 1;
        u64 memoryWrite  : 1;
        u64 memoryReadB  : 1;
        u64 memoryReadA  : 1;
        u64 selectIO     : 3;
        u64 selectMem    : 3;
        u64 selectAluB   : 3;
        u64 selectAluA   : 3;
    };
    u64 totalOp;
} OpCode;
#pragma pack(pop)

#define OPC_IMM_OFFS    0
#define OPC_IMM_MASK    (((1LLU << OPC_IMM_BITS) - 1) << OPC_IMM_OFFS)
#define OPC_ADDRB_BITS  9
#define OPC_ADDRB_OFFS  23
#define OPC_ADDRB_MASK  (((1LLU << OPC_ADDRB_BITS) - 1) << OPC_ADDRB_OFFS)
#define OPC_ADDRA_BITS  9
#define OPC_ADDRA_OFFS  (OPC_ADDRB_BITS + OPC_ADDRB_OFFS)
#define OPC_ADDRA_MASK  (((1LLU << OPC_ADDRA_BITS) - 1) << OPC_ADDRA_OFFS)
#define OPC_ALUOP_BITS  7
#define OPC_ALUOP_OFFS  (OPC_ADDRA_BITS + OPC_ADDRA_OFFS)
#define OPC_ALUOP_MASK  (((1LLU << OPC_ALUOP_BITS) - 1) << OPC_ALUOP_OFFS)
#define OPC_USEB_OFFS   (OPC_ALUOP_BITS + OPC_ALUOP_OFFS)
#define OPC_USEB_MASK   (1LLU << OPC_USEB_OFFS)
#define OPC_MEMWR_OFFS  (1 + OPC_USEB_OFFS)
#define OPC_MEMWR_MASK  (1LLU << OPC_MEMWR_OFFS)
#define OPC_MEMRB_OFFS  (1 + OPC_MEMWR_OFFS)
#define OPC_MEMRB_MASK  (1LLU << OPC_MEMRB_OFFS)
#define OPC_MEMRA_OFFS  (1 + OPC_MEMRB_OFFS)
#define OPC_MEMRA_MASK  (1LLU << OPC_MEMRA_OFFS)
#define OPC_SELIO_BITS  3
#define OPC_SELIO_OFFS  (1 + OPC_MEMRA_OFFS)
#define OPC_SELIO_MASK  (((1LLU << OPC_SELIO_BITS) - 1) << OPC_SELIO_OFFS)
#define OPC_SELMEM_BITS 3
#define OPC_SELMEM_OFFS (OPC_SELIO_BITS + OPC_SELIO_OFFS)
#define OPC_SELMEM_MASK (((1LLU << OPC_SELMEM_BITS) - 1) << OPC_SELMEM_OFFS)
#define OPC_SELALB_BITS 3
#define OPC_SELALB_OFFS (OPC_SELMEM_BITS + OPC_SELMEM_OFFS)
#define OPC_SELALB_MASK (((1LLU << OPC_SELALB_BITS) - 1) << OPC_SELALB_OFFS)
#define OPC_SELALA_BITS 3
#define OPC_SELALA_OFFS (OPC_SELALB_BITS + OPC_SELALB_OFFS)
#define OPC_SELALA_MASK (((1LLU << OPC_SELALA_BITS) - 1) << OPC_SELALA_OFFS)

#define OPC_SEL_MASK    (OPC_SELALA_MASK | OPC_SELALB_MASK | OPC_SELMEM_MASK | OPC_SELIO_MASK)
#define OPC_MEMCTL_MASK (OPC_MEMRA_MASK | OPC_MEMRB_MASK | OPC_MEMWR_MASK)

internal inline u32
get_offset_immediate(OpCodeStats *stats)
{
    u32 offset = 0;
    u32 maxImmAddr = maximum(stats->immediateBits, stats->addressBits);
    if (maxImmAddr > stats->immediateBits)
    {
        offset = maxImmAddr - stats->immediateBits;
    }
    return offset;
}

internal inline u32
get_offset_addr_b(OpCodeStats *stats)
{
    u32 offset = 0;
    u32 maxImmAddr = maximum(stats->immediateBits, stats->addressBits);
    if (maxImmAddr > stats->addressBits)
    {
        offset = maxImmAddr - stats->addressBits;
    }
    return offset;
}

internal inline u32
get_offset_addr_a(OpCodeStats *stats)
{
    u32 offset = 0;
    u32 maxImmAddr = maximum(stats->immediateBits, stats->addressBits);
    offset += maxImmAddr;
    return offset;
}

internal inline u32
get_offset_alu_op(OpCodeStats *stats)
{
    u32 offset = get_offset_addr_a(stats);
    offset += stats->addressBits;
    return offset;
}

internal inline u32
get_offset_useB(OpCodeStats *stats)
{
    u32 offset = get_offset_alu_op(stats);
    offset += stats->aluOpBits;
    return offset;
}

internal inline u32
get_offset_mem_write(OpCodeStats *stats)
{
    u32 offset = get_offset_useB(stats);
    offset += 1;
    return offset;
}

internal inline u32
get_offset_mem_read_b(OpCodeStats *stats)
{
    u32 offset = get_offset_mem_write(stats);
    offset += 1;
    return offset;
}

internal inline u32
get_offset_mem_read_a(OpCodeStats *stats)
{
    u32 offset = get_offset_mem_read_b(stats);
    offset += 1;
    return offset;
}

internal inline u32
get_offset_sel_io(OpCodeStats *stats)
{
    u32 offset = get_offset_mem_read_a(stats);
        offset += 1;
    return offset;
}

internal inline u32
get_offset_sel_mem(OpCodeStats *stats)
{
    u32 offset = get_offset_sel_io(stats);
    offset += stats->selectBits;
    return offset;
}

internal inline u32
get_offset_sel_alu_b(OpCodeStats *stats)
{
    u32 offset = get_offset_sel_mem(stats);
    offset += stats->selectBits;
    return offset;
}

internal inline u32
get_offset_sel_alu_a(OpCodeStats *stats)
{
    u32 offset = get_offset_sel_alu_b(stats);
    offset += stats->selectBits;
    return offset;
}

internal inline u64
opcode_packing(OpCodeStats *stats, OpCodeBuild *opCode)
{
    u64 result = 0;
    
    // TODO(michiel): Masking?
    if (opCode->immediate)
    {
        result |= (opCode->immediate << get_offset_immediate(stats));
    }
    if (opCode->memoryAddrB)
    {
        result |= (opCode->memoryAddrB << get_offset_addr_b(stats));
    }
    result |= (opCode->memoryAddrA << get_offset_addr_a(stats));
    result |= ((opCode->useB) << get_offset_useB(stats));
    result |= ((opCode->memoryWrite) << get_offset_mem_write(stats));
    result |= ((opCode->memoryReadA) << get_offset_mem_read_a(stats));
    result |= ((opCode->memoryReadB) << get_offset_mem_read_b(stats));
    result |= (opCode->aluOperation << get_offset_alu_op(stats));
    result |= (opCode->selectAluA << get_offset_sel_alu_a(stats));
    result |= (opCode->selectAluB << get_offset_sel_alu_b(stats));
    result |= (opCode->selectMem << get_offset_sel_mem(stats));
    result |= (opCode->selectIO << get_offset_sel_io(stats));
    
    return result;
}

#include "./vhdl_generator.c"

internal char *
select_to_string(enum Selection select)
{
    char *result = "";
    switch (select)
    {
        case Select_Zero:      { result = "Zero"; } break;
        case Select_MemoryA:   { result = "Memory A"; } break;
        case Select_MemoryB:   { result = "Memory B"; } break;
        case Select_Immediate: { result = "Immediate"; } break;
        case Select_IO:        { result = "IO"; } break;
        case Select_Alu:       { result = "Alu Out"; } break;
        INVALID_DEFAULT_CASE;
    }
    return result;
}

internal char *
alu_op_to_string(enum AluOp aluOp)
{
    char *result = "";
    switch (aluOp)
    {
        case Alu_Noop: { result = "NoOp"; } break;
        case Alu_And:  { result = "And"; } break;
        case Alu_Or:   { result = "Or"; } break;
        case Alu_Add:  { result = "Add"; } break;
        INVALID_DEFAULT_CASE;
    }
    return result;
}

internal void
print_opcode(OpCode opcode, b32 verbose)
{
    fprintf(stdout, "OpCode: 0x%016lX\n", opcode.totalOp);
    if (verbose)
    {
        fprintf(stdout, "  Alu A  : %s\n", select_to_string(opcode.selectAluA));
        fprintf(stdout, "  Alu B  : %s\n", select_to_string(opcode.selectAluB));
        fprintf(stdout, "  Mem in : %s\n", select_to_string(opcode.selectMem));
        fprintf(stdout, "  IO     : %s\n", select_to_string(opcode.selectIO));
        fprintf(stdout, "  Mem op : %s%s%s\n", 
                opcode.memoryReadA ? "read A " : "", opcode.memoryReadB ? "read B " : "", 
                opcode.memoryWrite ? "| write" : (!(opcode.memoryReadA || opcode.memoryReadB) ? "none" : ""));
        fprintf(stdout, "  Use B  : %s\n", opcode.useMemoryB ? "yes" : "no");
        fprintf(stdout, "  Alu op : %s\n", alu_op_to_string(opcode.aluOperation));
        fprintf(stdout, "  Addr A : %u\n", opcode.memoryAddrA);
        if (opcode.useMemoryB)
        {
            fprintf(stdout, "  Addr B : %u\n", opcode.memoryAddrB);
            fprintf(stdout, "  Padding: %u\n", opcode.padding);
        }
        else
        {
            fprintf(stdout, "  Imm    : %u\n", opcode.immediate);
        }
    }
}

global u32 gRegisterCount;
global Map gRegisterMap_ = {0};
global Map *gRegisterMap = &gRegisterMap_;

internal OpCodeStats
get_opcode_stats(u32 opCount, OpCodeBuild *opCodes)
{
    OpCodeStats result = {0};
    
    result.maxSelect = Select_Count;
    result.selectBits = log2_up(Select_Count);
    result.maxAluOp = Alu_Count;
    result.aluOpBits = log2_up(Alu_Count);
    result.maxImmediate = opc_max_immediate(opCount, opCodes);
    result.immediateBits = log2_up(result.maxImmediate);
    result.maxAddress = opc_max_address(opCount, opCodes);
    result.addressBits = log2_up(result.maxAddress);
    
    result.opCodeCount = opCount;
    result.opCodeBits = log2_up(opCount);
    
    result.opCodeBitWidth = result.selectBits * 4 + result.aluOpBits + maximum(result.immediateBits, result.addressBits) + result.addressBits + 3 + 1;
    
    return result;
}

internal inline u32
get_register_location(String varName)
{
    u32 *result = map_get(gRegisterMap, varName.data);
    if (!result)
    {
        result = allocate_size(sizeof(u32), 0);
        *result = gRegisterCount++;
        map_put(gRegisterMap, varName.data, result);
    }
    i_expect(*result < REG_MAX);
    return *result;
}

internal void
push_expression(Expression *expr, OpCode **opCodes, OpCodeBuild **opCodeBuilds)
{
    OpCode packedLoadOp = {0};
    OpCode packedOp = {0};
    packedLoadOp.selectAluA = Select_Alu;
    
    OpCodeBuild loadOp = {0};
    OpCodeBuild op = {0};
    loadOp.selectAluA = Select_Alu;
    
    if (expr->leftKind == EXPRESSION_VAR)
    {
        Variable *var = expr->left;
        if (var->kind == VARIABLE_IDENTIFIER)
        {
            i_expect(var->id->name.size);
            String idString = var->id->name;
            
            if (strings_are_equal(idString, str_internalize_cstring("IO")))
            {
                packedOp.selectAluA = Select_IO;
                op.selectAluA = Select_IO;
            }
            else
            {
                u32 regLoc = get_register_location(idString);
                
                packedLoadOp.memoryReadA = true;
                packedLoadOp.memoryAddrA = regLoc;
                packedOp.selectAluA = Select_MemoryA;
                
                loadOp.memoryReadA = true;
                loadOp.memoryAddrA = regLoc;
                op.selectAluA = Select_MemoryA;
                                     }
        }
        else
        {
            i_expect(var->kind == VARIABLE_CONSTANT);
            i_expect(var->constant->value < U32_MAX);
            
            packedOp.selectAluA = Select_Immediate;
            packedOp.immediate = var->constant->value;
            
            op.selectAluA = Select_Immediate;
            op.immediate = var->constant->value;
        }
    }
    else
    {
        i_expect(expr->leftKind == EXPRESSION_EXPR);
        push_expression(expr->leftExpr, opCodes, opCodeBuilds);
        packedOp.selectAluA = Select_Alu;
        op.selectAluA = Select_Alu;
    }
    
    if ((expr->op == EXPR_OP_AND) ||
        (expr->op == EXPR_OP_OR) ||
        (expr->op == EXPR_OP_ADD))
    {
        if (expr->rightKind == EXPRESSION_VAR)
        {
            Variable *var = expr->right;
            if (var->kind == VARIABLE_IDENTIFIER)
            {
                i_expect(var->id->name.size);
                String idString = var->id->name;
                
                if (strings_are_equal(idString, str_internalize_cstring("IO")))
                {
                    packedOp.selectAluB = Select_IO;
                    op.selectAluB = Select_IO;
                }
                else
                {
                    u32 regLoc = get_register_location(idString);
                    
                    packedLoadOp.useMemoryB = true;
                    packedLoadOp.memoryReadB = true;
                    packedLoadOp.memoryAddrB = regLoc;
                    packedOp.selectAluB = Select_MemoryB;
                    
                    loadOp.useB = true;
                    loadOp.memoryReadB = true;
                    loadOp.memoryAddrB = regLoc;
                    op.selectAluB = Select_MemoryB;
                }
            }
            else
            {
                i_expect(var->kind == VARIABLE_CONSTANT);
                i_expect(op.selectAluA != Select_Immediate);
                i_expect(var->constant->value < U32_MAX);
                
                packedOp.selectAluB = Select_Immediate;
                packedOp.immediate = var->constant->value;
                
                op.selectAluB = Select_Immediate;
                op.immediate = var->constant->value;
            }
        }
        else
        {
            i_expect(expr->rightKind == EXPRESSION_EXPR);
            i_expect(op.selectAluA != Select_Alu);
            push_expression(expr->rightExpr, opCodes, opCodeBuilds);
            
            packedOp.selectAluB = Select_Alu;
            op.selectAluB = Select_Alu;
        }
        
        switch (expr->op)
        {
            case EXPR_OP_AND: { op.aluOperation = Alu_And; } break;
            case EXPR_OP_OR:  { op.aluOperation = Alu_Or; } break;
            case EXPR_OP_ADD: { op.aluOperation = Alu_Add; } break;
            INVALID_DEFAULT_CASE;
        }
        packedOp.aluOperation = op.aluOperation;
    }
    
    if (opc_has_data(&loadOp))
    {
        buf_push(*opCodes, packedLoadOp);
        buf_push(*opCodeBuilds, loadOp);
    }
    buf_push(*opCodes, packedOp);
    buf_push(*opCodeBuilds, op);
}

internal void
push_assignment(Assignment *assign, OpCode **opCodes, OpCodeBuild **opCodeBuilds)
{
    push_expression(assign->expr, opCodes, opCodeBuilds);
    
    OpCodeBuild store = {0};
    OpCode packedStore = {0};
    
    i_expect(assign->id->name.size);
    String idString = assign->id->name;
    
    if (strings_are_equal(idString, str_internalize_cstring("IO")))
    {
        packedStore.selectIO = Select_Alu;
    store.selectIO = Select_Alu;
        }
    else
    {
        u32 regLoc = get_register_location(idString);
        
        packedStore.selectMem = Select_Alu;
    packedStore.memoryWrite = true;
        packedStore.memoryAddrA = regLoc;
        
        store.selectMem = Select_Alu;
        store.memoryWrite = true;
        store.memoryAddrA = regLoc;
        }
    
    buf_push(*opCodes, packedStore);
buf_push(*opCodeBuilds, store);
    }

internal void
optimize_assignments(OpCode **opCodes, OpCodeBuild **opCodeBuilds)
{
    // NOTE(michiel): Removes alu no-ops to assignment selection
    u32 opCodeCount = buf_len(*opCodeBuilds);
    for (u32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCodeBuild cur = (*opCodeBuilds)[opIdx];
        OpCodeBuild prev = (*opCodeBuilds)[opIdx - 1];
        
        //OpCode packedCur = (*opCodes)[opIdx];
        OpCode packedPrev = (*opCodes)[opIdx - 1];
        
        if (opc_only_selection(&cur))
        {
            // NOTE(michiel): Only selection is set for cur
            if (opc_only_selection(&prev) && 
                prev.selectAluA && !prev.selectAluB && 
                !prev.selectMem && !prev.selectIO)
            {
                // NOTE(michiel): Only alu A selection with a alu no-op for prev
                
                // TODO(michiel): Is this correct??
                if (cur.selectAluA)
                {
                    // NOTE(michiel): Nothing to do
                }
                else if (cur.selectAluB)
                {
                    packedPrev.selectAluB = packedPrev.selectAluA;
                    prev.selectAluB = prev.selectAluA;
                }
                else if (cur.selectMem)
                {
                    packedPrev.selectMem = packedPrev.selectAluA;
                    prev.selectMem = prev.selectAluA;
                }
                else 
                {
                    i_expect(cur.selectIO);
                    packedPrev.selectIO = packedPrev.selectAluA;
                    prev.selectIO = prev.selectAluA;
                }
                packedPrev.selectAluA = Select_Zero;
                prev.selectAluA = Select_Zero;
                
                for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                {
                    (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                    (*opCodeBuilds)[popIdx] = (*opCodeBuilds)[popIdx + 1];
                }
                --buf_len_(*opCodes);
                --buf_len_(*opCodeBuilds);
                --opCodeCount;
                --opIdx;
                (*opCodes)[opIdx] = packedPrev;
                (*opCodeBuilds)[opIdx] = prev;
            }
        }
    }
}

internal void
optimize_read_setups(OpCode **opCodes, OpCodeBuild **opCodeBuilds)
{
    // NOTE(michiel): Combines read setup with previous instructions that don't do
    // memory manipulation or only do a write to the same address.
    // It's a little more involved cause it also depends on the next instructions use
    // of the read.
    u32 opCodeCount = buf_len(*opCodeBuilds);
    for (u32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCodeBuild cur = (*opCodeBuilds)[opIdx];
        OpCodeBuild prev = (*opCodeBuilds)[opIdx - 1];
        
        OpCode packedCur = (*opCodes)[opIdx];
        OpCode packedPrev = (*opCodes)[opIdx - 1];
        
        if (cur.memoryReadA && !cur.memoryReadB && !cur.memoryWrite &&
            ((!cur.selectAluA && !cur.selectAluB &&
             !cur.selectMem && !cur.selectIO) ||
             (((cur.selectAluA == Select_Alu) &&
              !cur.selectAluB && !cur.selectMem && !cur.selectIO) &&
              (cur.aluOperation == Alu_Noop))))
        {
            // NOTE(michiel): Read setup for mem A
            
    if (!prev.memoryReadA && !prev.memoryReadB && !prev.memoryWrite)
            {
                // NOTE(michiel): If previous doesn't use memory
                packedPrev.memoryReadA = packedCur.memoryReadA;
        packedPrev.memoryAddrA = packedCur.memoryAddrA;
        
        prev.memoryReadA = cur.memoryReadA;
        prev.memoryAddrA = cur.memoryAddrA;
                
                for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                {
            (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
            (*opCodeBuilds)[popIdx] = (*opCodeBuilds)[popIdx + 1];
        }
        --buf_len_(*opCodes);
                --buf_len_(*opCodeBuilds);
                --opCodeCount;
                --opIdx;
        (*opCodes)[opIdx] = packedPrev;
        (*opCodeBuilds)[opIdx] = prev;
            }
            else if (prev.memoryWrite && !prev.memoryReadA && !prev.memoryReadB &&
                     (prev.memoryAddrA == cur.memoryAddrA))
            {
                // NOTE(michiel): Previous was write to the same memory address
                
                if (opIdx < (opCodeCount - 1))
                {
            OpCodeBuild next = (*opCodeBuilds)[opIdx + 1];
            OpCode packedNext = (*opCodes)[opIdx + 1];
                    
                    if ((next.selectAluA == Select_MemoryA) ||
                        (next.selectAluB == Select_MemoryA) ||
                        (next.selectMem == Select_MemoryA) ||
                        (next.selectIO == Select_MemoryA))
                    {
                    if (next.selectAluA == Select_MemoryA)
                    {
                    packedNext.selectAluA = Select_Alu;
                    next.selectAluA = Select_Alu;
                        }
                        else if (next.selectAluB == Select_MemoryA)
                        {
                    packedNext.selectAluB = Select_Alu;
                    next.selectAluB = Select_Alu;
                            }
                        else if (next.selectMem == Select_MemoryA)
                        {
                    packedNext.selectMem = Select_Alu;
                    next.selectMem = Select_Alu;
                        }
                        else
                        {
                            i_expect(next.selectIO == Select_MemoryA);
                    packedNext.selectIO = Select_Alu;
                    next.selectIO = Select_Alu;
                        }
                        
                        packedPrev.selectAluA = Select_Alu;
                prev.selectAluA = Select_Alu;
                
                        for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                {
                    (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                            (*opCodeBuilds)[popIdx] = (*opCodeBuilds)[popIdx + 1];
                        }
                --buf_len_(*opCodes);
                --buf_len_(*opCodeBuilds);
                        --opCodeCount;
                        --opIdx;
                        (*opCodes)[opIdx] = packedPrev;
                (*opCodes)[opIdx + 1] = packedNext;
                (*opCodeBuilds)[opIdx] = prev;
                (*opCodeBuilds)[opIdx + 1] = next;
                    }
                }
            }
        }
    }
}

internal void
optimize_combine_write(OpCode **opCodes, OpCodeBuild **opCodeBuilds)
{
    // NOTE(michiel): Combines a alu operation on a previous output _if_ the
    // previous output was generated by a alu no-op.
    u32 opCodeCount = buf_len(*opCodeBuilds);
    for (u32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCodeBuild cur = (*opCodeBuilds)[opIdx];
        OpCodeBuild prev = (*opCodeBuilds)[opIdx - 1];
        
        OpCode packedCur = (*opCodes)[opIdx];
        OpCode packedPrev = (*opCodes)[opIdx - 1];
        
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
                packedPrev.aluOperation = packedCur.aluOperation;
                prev.aluOperation = cur.aluOperation;
                
                if (cur.selectAluA == Select_Alu)
                {
                    packedPrev.selectAluB = packedCur.selectAluB;
                    prev.selectAluB = cur.selectAluB;
                }
                else
                {
                    packedPrev.selectAluB = packedCur.selectAluA;
                    prev.selectAluB = cur.selectAluA;
                }
                packedPrev.immediate = packedCur.immediate;
                prev.immediate = cur.immediate;
                
                for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                {
                    (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                    (*opCodeBuilds)[popIdx] = (*opCodeBuilds)[popIdx + 1];
                }
                --buf_len_(*opCodes);
                --buf_len_(*opCodeBuilds);
                --opCodeCount;
                --opIdx;
                (*opCodes)[opIdx] = packedPrev;
                (*opCodeBuilds)[opIdx] = prev;
            }
        }
    }
}

int main(int argc, char **argv)
{
    int errors = 0;
    
    FileStream outputStream = {0};
    outputStream.file = stdout;
    // outputStream.verbose = true;
    
    if (argc == 2)
    {
        //fprintf(stdout, "Tokenize file: %s\n", argv[1]);
        
        Token *tokens = tokenize_file(argv[1]);
        Program *program = parse(tokens);
        OpCode *opCodes = 0;
        OpCodeBuild *opCodeBuilds = 0;
        
        // print_tokens(tokens);
        print_parsed_program(outputStream, program);
        
        for (u32 stmtIdx = 0; stmtIdx < program->nrStatements; ++stmtIdx)
        {
            Statement *statement = program->statements + stmtIdx;
            
            if (statement->kind == STATEMENT_ASSIGN)
            {
                push_assignment(statement->assign, &opCodes, &opCodeBuilds);
            }
            else
            {
                i_expect(statement->kind == STATEMENT_EXPR);
                push_expression(statement->expr, &opCodes, &opCodeBuilds);
            }
        }
        
        //fprintf(stdout, "Pre Opcodes: %u\n", buf_len(opCodes));
        optimize_assignments(&opCodes, &opCodeBuilds);
        optimize_read_setups(&opCodes, &opCodeBuilds);
        optimize_combine_write(&opCodes, &opCodeBuilds);
        //fprintf(stdout, "Post Opcodes: %u\n", buf_len(opCodes));
        
#if 0
        for (u32 opIdx = 0; opIdx < buf_len(opCodes); ++opIdx)
        {
            OpCode opCode = opCodes[opIdx];
            print_opcode(opCode, true);
        }
#else
        OpCodeStats opCodeStats = get_opcode_stats(buf_len(opCodeBuilds), opCodeBuilds);
        fprintf(stdout, "Stats:\n");
        fprintf(stdout, "  SEL: Max = %u, Bits = %u\n", opCodeStats.maxSelect, opCodeStats.selectBits);
        fprintf(stdout, "  ALU: Max = %u, Bits = %u\n", opCodeStats.maxAluOp, opCodeStats.aluOpBits);
        fprintf(stdout, "  IMM: Max = %u, Bits = %u\n", opCodeStats.maxImmediate, opCodeStats.immediateBits);
        fprintf(stdout, "  ADR: Max = %u, Bits = %u\n", opCodeStats.maxAddress, opCodeStats.addressBits);
        
        opCodeStats.bitWidth = 8;
        
        FileStream opCodeStream = {0};
        opCodeStream.file = fopen("gen_opcodes.vhd", "wb");
        //generate_opcode_vhdl_(opCodes, opCodeStream);
        generate_opcode_vhdl(&opCodeStats, opCodeBuilds, opCodeStream);
        fclose(opCodeStream.file);
        
        opCodeStream.file = fopen("gen_controller.vhd", "wb");
        generate_controller(&opCodeStats, opCodeStream);
        fclose(opCodeStream.file);
        
        opCodeStream.file = fopen("gen_constants.vhd", "wb");
        generate_constants(&opCodeStats, opCodeStream);
        fclose(opCodeStream.file);
        
        opCodeStream.file = fopen("gen_registers.vhd", "wb");
        generate_registers(&opCodeStats, opCodeStream);
        fclose(opCodeStream.file);
        
        opCodeStream.file = fopen("gen_cpu.vhd", "wb");
        generate_cpu_main(&opCodeStats, opCodeStream);
        fclose(opCodeStream.file);
#endif
    }
    else
    {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        errors = 1;
    }
    
    return errors;
}
