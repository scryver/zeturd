#include "./common.h"
#include "./tokenizer.h"
#include "./ast.h"
#include "./parser.h"

#include "./common.c"
#include "./tokenizer.c"
#include "./ast.c"
#include "./parser.c"
#include "./intermediaterep.c"

#define REG_MAX (1 << 9)

typedef enum Selection
{
    Select_Zero,
    Select_MemoryA,
    Select_MemoryB,
    Select_Immediate,
    Select_IO,
    Select_Alu,
    
    Select_Count,
} Selection;

global char *gSelectionNames[Select_Count] = 
{
    [Select_Zero] = "Zero",
    [Select_MemoryA] = "MemA",
    [Select_MemoryB] = "MemB",
    [Select_Immediate] = "Imm",
    [Select_IO] = "In",
    [Select_Alu] = "Alu",
};

typedef enum AluOp
{
    Alu_Noop,
    Alu_Or,
    Alu_Xor,
    Alu_And,
    Alu_Add,
    Alu_Sub,
    
    Alu_Count,
} AluOp;

global char *gAluOpNames[Alu_Count] = 
{
    [Alu_Noop] = "Thru",
    [Alu_Or] = "Or",
    [Alu_Xor] = "Xor",
    [Alu_And] = "And",
    [Alu_Add] = "Add",
    [Alu_Sub] = "Sub",
};

typedef struct Alu
{
     Selection inputA;
     Selection inputB;
     AluOp op;
} Alu;

typedef struct Register
{
    union
    {
    struct
    {
            b8 write;
            b8 readA;
            b8 readB;
    };
        u32 control;
    };
    
     Selection input;
    u32 wAddr;
    u32 rAddrA;
    u32 rAddrB;
} Register;

typedef struct IOOut
{
     Selection output;
} IOOut;

typedef struct OpCodeEntry
{
    // NOTE(michiel): This structure doesn't account for clocking, so memory reads are expected
    // to be finished before usage in alu or io out.
    b32      useMemory;
    Register memory;
    
    b32      useAlu;
    Alu      alu;
    
    b32      useIOIn;
b32      useIOOut;
    IOOut    output;
    
    b32      useImmediate;
    s32      immediate;
    } OpCodeEntry;

typedef struct OpCode
{
    enum Selection selectAluA;
    enum Selection selectAluB;
    enum Selection selectMem;
    enum Selection selectIO;
    
    enum AluOp aluOperation;
    
    b32 memoryReadA;
    b32 memoryReadB;
    b32 memoryWrite;
    
    s32 immediate;
    u32 memoryAddrA;
    u32 memoryAddrB;
} OpCode;

typedef struct OpCodeStats
{
    b32 synced;
    
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

typedef struct OpCodeBuilder
{
    //u32 opCodeCount;
    OpCodeEntry *entries;
    
    u32 registerCount;
    Map registerMap;
    
    OpCodeStats stats;
} OpCodeBuilder;

#include "./opc_builder.c"

internal b32 opc_only_selection(OpCode *opCode)
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

internal b32 opc_has_select(OpCode *opCode, enum Selection select)
{
    b32 result = false;
    if ((opCode->selectAluA == select) ||
        (opCode->selectAluB == select) ||
        (opCode->selectMem == select) ||
        (opCode->selectIO == select))
    {
        result = true;
    }
    return result;
}

internal b32 opc_has_data(OpCode *opCode)
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

internal u32 opc_max_immediate(u32 opCount, OpCode *opCodes)
{
    u32 maxImmediate = 0;
    for (u32 opIdx = 0; opIdx < opCount; ++opIdx)
    {
        OpCode opCode = opCodes[opIdx];
        if (maxImmediate < (u32)opCode.immediate)
        {
            maxImmediate = (u32)opCode.immediate;
        }
    }
    return maxImmediate;
}

internal s32 opc_max_address(u32 opCount, OpCode *opCodes)
{
    s32 maxAddress = -1;
    for (u32 opIdx = 0; opIdx < opCount; ++opIdx)
    {
        OpCode opCode = opCodes[opIdx];
        if ((opCode.memoryReadA || opCode.memoryWrite) && 
            (maxAddress < safe_truncate_to_s32(opCode.memoryAddrA)))
        {
            maxAddress = opCode.memoryAddrA;
        }
        if (opCode.memoryReadB && (maxAddress < safe_truncate_to_s32(opCode.memoryAddrB)))
        {
            maxAddress = opCode.memoryAddrB;
        }
    }
    return maxAddress;
}

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
opcode_packing(OpCodeStats *stats, OpCode *opCode)
{
    u64 result = 0;
    
    // TODO(michiel): Masking?
    if (opCode->immediate)
    {
        result |= ((opCode->immediate & ((1ULL << stats->bitWidth) - 1)) << get_offset_immediate(stats));
    }
    else if (opCode->memoryAddrB)
    {
        result |= ((u64)opCode->memoryAddrB << get_offset_addr_b(stats));
    }
    result |= ((u64)opCode->memoryAddrA << get_offset_addr_a(stats));
    result |= ((u64)(opCode->memoryReadB ? 1 : 0) << get_offset_useB(stats)); // TODO(michiel): Remove
    result |= ((u64)(opCode->memoryWrite) << get_offset_mem_write(stats));
    result |= ((u64)(opCode->memoryReadA) << get_offset_mem_read_a(stats));
    result |= ((u64)(opCode->memoryReadB) << get_offset_mem_read_b(stats));
    result |= ((u64)opCode->aluOperation << get_offset_alu_op(stats));
    result |= ((u64)opCode->selectAluA << get_offset_sel_alu_a(stats));
    result |= ((u64)opCode->selectAluB << get_offset_sel_alu_b(stats));
    result |= ((u64)opCode->selectMem << get_offset_sel_mem(stats));
    result |= ((u64)opCode->selectIO << get_offset_sel_io(stats));
    
    return result;
}

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
        case Alu_Or:   { result = "Or"; } break;
        case Alu_Xor:  { result = "Xor"; } break;
        case Alu_And:  { result = "And"; } break;
        case Alu_Add:  { result = "Add"; } break;
        case Alu_Sub:  { result = "Sub"; } break;
        INVALID_DEFAULT_CASE;
    }
    return result;
}

internal void
print_opcode_single(OpCode *opcode)
{
    fprintf(stdout, "OpCode:\n");
    fprintf(stdout, "  Alu A  : %s\n", select_to_string(opcode->selectAluA));
    fprintf(stdout, "  Alu B  : %s\n", select_to_string(opcode->selectAluB));
    fprintf(stdout, "  Mem in : %s\n", select_to_string(opcode->selectMem));
    fprintf(stdout, "  IO     : %s\n", select_to_string(opcode->selectIO));
    fprintf(stdout, "  Mem op : %s%s%s\n",
            opcode->memoryReadA ? "read A " : "", opcode->memoryReadB ? "read B " : "",
            opcode->memoryWrite ? "| write" : (!(opcode->memoryReadA || opcode->memoryReadB) ? "none" : ""));
    fprintf(stdout, "  Use B  : %s\n", opcode->memoryReadB ? "yes" : "no");
    fprintf(stdout, "  Alu op : %s\n", alu_op_to_string(opcode->aluOperation));
    fprintf(stdout, "  Addr A : %u\n", opcode->memoryAddrA);
    if (opcode->memoryReadB)
    {
        fprintf(stdout, "  Addr B : %u\n", opcode->memoryAddrB);
    }
    else
    {
        fprintf(stdout, "  Imm    : %u\n", opcode->immediate);
    }
}

#include "./vhdl_generator.c"
#include "./graphvizu.c"
#include "./graph_tokens.c"
#include "./graph_ast.c"
#include "./simulator.c"
#include "./optimizer.c"

internal void
print_opcode(FileStream output, OpCodeStats *stats, OpCode *opcode)
{
    fprintf(output.file, "OpCode: 0x%.*lX\n",
            (get_offset_sel_alu_a(stats) + stats->selectBits + 3) / 4,
            opcode_packing(stats, opcode));
    if (output.verbose)
    {
        fprintf(output.file, "  Alu A  : %s\n", select_to_string(opcode->selectAluA));
        fprintf(output.file, "  Alu B  : %s\n", select_to_string(opcode->selectAluB));
        fprintf(output.file, "  Mem in : %s\n", select_to_string(opcode->selectMem));
        fprintf(output.file, "  IO     : %s\n", select_to_string(opcode->selectIO));
        fprintf(output.file, "  Mem op : %s%s%s\n",
                opcode->memoryReadA ? "read A " : "", opcode->memoryReadB ? "read B " : "",
                opcode->memoryWrite ? "| write" : (!(opcode->memoryReadA || opcode->memoryReadB) ? "none" : ""));
        fprintf(output.file, "  Use B  : %s\n", opcode->memoryReadB ? "yes" : "no");
        fprintf(output.file, "  Alu op : %s\n", alu_op_to_string(opcode->aluOperation));
        fprintf(output.file, "  Addr A : %u\n", opcode->memoryAddrA);
        if (opcode->memoryReadB)
        {
            fprintf(output.file, "  Addr B : %u\n", opcode->memoryAddrB);
        }
        else
        {
            fprintf(output.file, "  Imm    : %u\n", opcode->immediate);
        }
    }
}

internal OpCodeStats
get_opcode_stats(u32 opCount, OpCode *opCodes, u32 bitWidth)
{
    OpCodeStats result = {0};
    
    result.bitWidth = bitWidth;
    
    result.maxSelect = Select_Count - 1;
    result.selectBits = log2_up(result.maxSelect);
    result.maxAluOp = Alu_Count - 1;
    result.aluOpBits = log2_up(result.maxAluOp);
    
    //result.maxImmediate = opc_max_immediate(opCount, opCodes);
    //result.immediateBits = log2_up(result.maxImmediate);
    result.maxImmediate = (u32)((1ULL << bitWidth) - 1);
    result.immediateBits = bitWidth;
    i_expect(opc_max_immediate(opCount, opCodes) <= result.maxImmediate);
    
    s32 maxAddress = opc_max_address(opCount, opCodes);
    if (maxAddress < 0)
    {
        result.maxAddress = 0;
        result.addressBits = 0;
    }
    else if (maxAddress == 0)
    {
        result.maxAddress = 0;
        result.addressBits = 1;
    }
    else
    {
        result.maxAddress = (u32)maxAddress;
    result.addressBits = log2_up(result.maxAddress);
    }
    fprintf(stdout, "Max addr: %u, addr bits: %u\n", result.maxAddress, result.addressBits);
    
    result.opCodeCount = opCount;
    result.opCodeBits = log2_up(opCount);
    
    result.opCodeBitWidth = result.selectBits * 4 + result.aluOpBits + maximum(result.immediateBits, result.addressBits) + result.addressBits + 3 + 1;
    
    return result;
}

#if 0
internal void
push_opc(OpCodeBuilder *builder, OpCodeEntry *entry)
{
    i_expect(builder->opCodeCount < builder->maxOpCodes);
    OpCodeEntry *op = builder->entries + builder->opCodeCount++;
    *op = *entry;
    // TODO(michiel): update stats
}

internal inline u32
get_register_address(Map *registerMap, String name, u32 *registerCount)
{
    u32 *result = map_get(registerMap, name.data);
    if (!result)
    {
        i_expect(*registerCount < MAX_REG);
        result = allocate_size(sizeof(u32), 0);
        *result = (*registerCount)++;
        map_put(registerMap, name.data, result);
    }
    i_expect(*result < REG_MAX);
    return *result;
}

internal void
build_expression(OpCodeBuilder *builder, Expr *expr)
{
    // TODO(michiel): Implement new expression tree
    OpCodeEntry op = {0};
    op.useAlu = true;
        
    if (expr->leftKind == EXPRESSION_VAR)
    {
        Variable *var = expr->left;
        if (var->kind == VARIABLE_IDENTIFIER)
        {
            String idString = var->id->name;
        i_expect(idString.size);
            if (strings_are_equal(idString, str_internalize_cstring("IO")))
            {
                op.alu.inputA = Select_IO;
                }
            else
            {
                op.useMemory = true;
                op.memory.memoryReadA = true;
                op.memory.readAddrA = get_register_address(&builder->registerMap, idString,
                                                            &builder->registerCount);
                op.alu.inputA = Select_MemoryA;
            }
            }
        else
        {
            i_expect(var->kind == VARIABLE_CONSTANT);
            i_expect(var->constant->value < U32_MAX);
            op.useImmediate = true;
            op.immediate = var->constant->value;
            op.alu.inputA = Select_Immediate;
            }
    }
    else
    {
        i_expect(expr->leftKind == EXPRESSION_EXPR);
        build_expression(builder, expr->leftExpr);
        op.alu.inputA = Select_Alu;
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
                String idString = var->id->name;
                i_expect(idString.size);
                if (strings_are_equal(idString, str_internalize_cstring("IO")))
                {
                    op.alu.inputB = Select_IO;
                }
                else
                {
                    op.useMemory = true;
                    op.memory.memoryReadB = true;
                    op.memory.readAddrB = get_register_address(&builder->registerMap, idString,
                                                               &builder->registerCount);
                    op.alu.inputB = Select_MemoryB;
                }
            }
            else
            {
                i_expect(var->kind == VARIABLE_CONSTANT);
                i_expect(var->constant->value < U32_MAX);
                op.useImmediate = true;
                op.immediate = var->constant->value;
                op.alu.inputB = Select_Immediate;
            }
        }
        else
        {
            i_expect(expr->rightKind == EXPRESSION_EXPR);
            build_expression(expr->rightExpr, builder);
            op.alu.inputB = Select_Alu;
        }
        
    }
    else
    {
        op.alu.op = Alu_Noop;
    }
    
    push_opc(builder, &op);
}
#endif

global u32 gRegisterCount;
global Map gRegisterMap_ = {0};
global Map *gRegisterMap = &gRegisterMap_;

internal inline u32
get_register_location(String varName)
{
    u32 *result = map_get(gRegisterMap, varName.data);
    if (!result)
    {
        i_expect(gRegisterCount < MAX_REG);
        result = allocate_size(sizeof(u32), 0);
        *result = gRegisterCount++;
        map_put(gRegisterMap, varName.data, result);
    }
    i_expect(*result < REG_MAX);
    return *result;
}

internal void
push_expression(Expression *expr, OpCode **opCodes)
{
    OpCode loadOp = {0};
    OpCode op = {0};
    //op.selectAluA = Select_Alu; // NOTE(michiel): Standard passthrough
    
    if (expr->leftKind == EXPRESSION_VAR)
    {
        Variable *var = expr->left;
        if (var->kind == VARIABLE_IDENTIFIER)
        {
            i_expect(var->id->name.size);
            String idString = var->id->name;
            if (strings_are_equal(idString, str_internalize_cstring("IO")))
            {
                op.selectAluA = Select_IO;
            }
            else
            {
                u32 regLoc = get_register_location(idString);
                loadOp.selectAluA = Select_Alu;
                loadOp.memoryReadA = true;
                loadOp.memoryAddrA = regLoc;
                op.selectAluA = Select_MemoryA;
            }
        }
        else
        {
            i_expect(var->kind == VARIABLE_CONSTANT);
            i_expect(var->constant->value < U32_MAX);
            
            op.selectAluA = Select_Immediate;
            op.immediate = var->constant->value;
        }
    }
    else
    {
        i_expect(expr->leftKind == EXPRESSION_EXPR);
        push_expression(expr->leftExpr, opCodes);
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
                    op.selectAluB = Select_IO;
                }
                else
                {
                    u32 regLoc = get_register_location(idString);
                    loadOp.selectAluA = Select_Alu;
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
                
                op.selectAluB = Select_Immediate;
                op.immediate = var->constant->value;
            }
        }
        else
        {
            i_expect(expr->rightKind == EXPRESSION_EXPR);
            i_expect(op.selectAluA != Select_Alu);
            push_expression(expr->rightExpr, opCodes);
            
            op.selectAluB = Select_Alu;
        }
        
        switch (expr->op)
        {
            case EXPR_OP_AND: { op.aluOperation = Alu_And; } break;
            case EXPR_OP_OR:  { op.aluOperation = Alu_Or; } break;
            case EXPR_OP_ADD: { op.aluOperation = Alu_Add; } break;
            INVALID_DEFAULT_CASE;
        }
    }
    
    if (opc_has_data(&loadOp))
    {
        buf_push(*opCodes, loadOp);
    }
    buf_push(*opCodes, op);
}

internal void
push_assignment(Assignment *assign, OpCode **opCodes)
{
    push_expression(assign->expr, opCodes);
    
    OpCode store = {0};
    store.selectAluA = Select_Alu;
    
    i_expect(assign->id->name.size);
    String idString = assign->id->name;
    
    if (strings_are_equal(idString, str_internalize_cstring("IO")))
    {
        store.selectIO = Select_Alu;
    }
    else
    {
        u32 regLoc = get_register_location(idString);
        
        store.selectMem = Select_Alu;
        store.memoryWrite = true;
        store.memoryAddrA = regLoc;
    }
    
    buf_push(*opCodes, store);
}

int main(int argc, char **argv)
{
    // TODO(michiel): ROM Tables
    // TODO(michiel): Make cordic example (slowly add iteration e.d.)
    int errors = 0;
    
    FileStream outputStream = {0};
    outputStream.file = stdout;
    // outputStream.verbose = true;
    
    if (argc == 2)
    {
        //fprintf(stdout, "Tokenize file: %s\n", argv[1]);
        
        Token *tokens = tokenize_file(argv[1]);
        if (tokens)
        {
            graph_tokens(tokens, "tokens.dot");
            StmtList *stmts = ast_from_tokens(tokens);
            
            AstOptimizer astOptimizer = {0};
            astOptimizer.statements = *stmts;
            ast_optimize(&astOptimizer);
            graph_ast(&astOptimizer.statements, "ast.dot");
            //print_ast((FileStream){.file=stdout}, stmts);
            generate_ir(&astOptimizer, (FileStream){.file=stdout});
            //generate_ir_file(&astOptimizer, "henkie.tst");
            
            u32 trimmed = 0;
            for (Expr *fre = astOptimizer.exprFreeList;
                 fre;
                 fre = fre->nextFree)
            {
                i_expect(fre->kind == Expr_None);
                ++trimmed;
            }
            fprintf(stdout, "Trimmed %d expressions\n", trimmed);
            
            // TODO(michiel): Make these out of the astOptimizer,
            // see generate_ir for proper handling of nested expressions
            OpCodeBuilder builder = {0};
            
            generate_opcodes(&builder, &astOptimizer);
            //print_opcodes(buf_len(builder.entries), builder.entries);
            
            OpCode *opCodes = layout_instructions(&builder);
            
             builder.stats = get_opcode_stats(buf_len(opCodes), opCodes, 32);
            builder.stats.synced = false;
            
            fprintf(stdout, "Stats:\n");
            fprintf(stdout, "  SEL: Max = %u, Bits = %u\n", builder.stats.maxSelect, builder.stats.selectBits);
            fprintf(stdout, "  ALU: Max = %u, Bits = %u\n", builder.stats.maxAluOp, builder.stats.aluOpBits);
            fprintf(stdout, "  IMM: Max = %u, Bits = %u\n", builder.stats.maxImmediate, builder.stats.immediateBits);
            fprintf(stdout, "  ADR: Max = %u, Bits = %u\n", builder.stats.maxAddress, builder.stats.addressBits);

#if 0            
            u32 inputs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
            simulate(&builder.stats, buf_len(opCodes), opCodes, array_count(inputs), inputs,
                     100);
            #endif
            
#if 0
            FileStream printStream = {0};
            printStream.file = fopen("opcodes.list", "wb");
            printStream.verbose = true;
            for (u32 opIdx = 0; opIdx < buf_len(opCodes); ++opIdx)
            {
                print_opcode(printStream, &builder.stats, opCodes + opIdx);
            }
#endif
            test_graph("testing.dot", builder.registerCount, buf_len(opCodes), opCodes, false);
            test_graph("opcodes.dot", builder.registerCount, buf_len(opCodes), opCodes, true);
            //save_graph("opcodes.dot", gRegisterCount, buf_len(opCodes), opCodes);
            
            FileStream opCodeStream = {0};
            opCodeStream.file = fopen("gen_opcodes.vhd", "wb");
            generate_opcode_vhdl(&builder.stats, opCodes, opCodeStream);
            fclose(opCodeStream.file);
            
            opCodeStream.file = fopen("gen_controller.vhd", "wb");
            generate_controller(&builder.stats, opCodeStream);
            fclose(opCodeStream.file);
            
            opCodeStream.file = fopen("gen_constants.vhd", "wb");
            generate_constants(&builder.stats, opCodeStream);
            fclose(opCodeStream.file);
            
            if (builder.stats.addressBits > 0)
            {
            opCodeStream.file = fopen("gen_registers.vhd", "wb");
            generate_registers(&builder.stats, opCodeStream);
            fclose(opCodeStream.file);
            }
            
            opCodeStream.file = fopen("gen_cpu.vhd", "wb");
            generate_cpu_main(&builder.stats, opCodeStream);
            fclose(opCodeStream.file);
            
        #if 0
        for (u32 stmtIdx = 0; stmtIdx < program->nrStatements; ++stmtIdx)
        {
            Statement *statement = program->statements + stmtIdx;
            
            if (statement->kind == STATEMENT_ASSIGN)
            {
                push_assignment(statement->assign, &opCodes);
            }
            else
            {
                i_expect(statement->kind == STATEMENT_EXPR);
                // NOTE(michiel): We expect single tweakable things here. All other
                // usefull statements are assignments
                
                if (statement->expr->leftKind == EXPRESSION_VAR)
                {
                    Variable *var = statement->expr->left;
                    if (var->kind == VARIABLE_IDENTIFIER)
                    {
                        i_expect(var->id->name.size);
                        String idString = var->id->name;
                        
                        if (strings_are_equal(idString, str_internalize_cstring("SYNCED")))
                        {
                            synced = true;
                        }
                    }
                }
            }
        }

            opc_optimize_unused_alu(buf_len(opCodes), opCodes);
            opc_optimize_mergers(buf_len(opCodes), opCodes);
            
#if 0
        u32 prevCount = buf_len(opCodes);
            fprintf(stdout, "Start optimize: %u\n", prevCount);
            s32 maxSame = 4;
            u32 iters = 0;
        for (;;)
        {
                optimize_alu_noop(&opCodes);
                fprintf(stdout, "After Alu Noop: %u\n", buf_len(opCodes));
                optimize_assignments(&opCodes);
                fprintf(stdout, "After Assign  : %u\n", buf_len(opCodes));
optimize_combine_write(&opCodes);
                fprintf(stdout, "After Comb Wr : %u\n", buf_len(opCodes));
                optimize_combine_read(&opCodes);
                fprintf(stdout, "After Comb Rd : %u\n", buf_len(opCodes));
                optimize_read_setups(&opCodes);
                fprintf(stdout, "After Read set: %u\n", buf_len(opCodes));
        
            u32 curCount = buf_len(opCodes);
                
                fprintf(stdout, "After Assign  : %u\n", buf_len(opCodes));
                fprintf(stdout, "After iter %3d: %u\n", ++iters, buf_len(opCodes));
            if (curCount == prevCount)
            {
                    --maxSame;
                }
                if (maxSame <= 0)
                {
                break;
            }
            prevCount = curCount;
        }
        #endif
            #endif
    }
        else
        {
            fprintf(stderr, "Could not find file: %s\n", argv[1]);
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        errors = 1;
    }
    
    return errors;
}
