#include "./common.h"
#include "./tokenizer.h"
#include "./ast.h"
#include "./parser.h"

#include "./common.c"
#include "./tokenizer.c"
#include "./ast.c"
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

typedef struct Alu
{
    enum Selection inputA;
    enum Selection inputB;
    enum AluOp op;
} Alu;

typedef struct Register
{
    union
    {
    struct
    {
            b8 memoryWrite;
            b8 memoryReadA;
            b8 memoryReadB;
    };
        u32 control;
    };
    
    enum Selection input;
    u32 writeAddr;
    u32 readAddrA;
    u32 readAddrB;
} Register;

typedef struct IOOut
{
    enum Selection output;
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
    u32      immediate;
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
    
    u32 immediate;
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
    u32 maxOpCodes;
    u32 opCodeCount;
    OpCodeEntry *entries;
    
    u32 registerCount;
    Map registerMap;
    
    OpCodeStats stats;
} OpCodeBuilder;

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
        if (maxImmediate < opCode.immediate)
        {
            maxImmediate = opCode.immediate;
        }
    }
    return maxImmediate;
}

internal u32 opc_max_address(u32 opCount, OpCode *opCodes)
{
    u32 maxAddress = 0;
    for (u32 opIdx = 0; opIdx < opCount; ++opIdx)
    {
        OpCode opCode = opCodes[opIdx];
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
        result |= (opCode->immediate << get_offset_immediate(stats));
    }
    if (opCode->memoryAddrB)
    {
        result |= (opCode->memoryAddrB << get_offset_addr_b(stats));
    }
    result |= (opCode->memoryAddrA << get_offset_addr_a(stats));
    result |= ((opCode->memoryReadB ? 1 : 0) << get_offset_useB(stats)); // TODO(michiel): Remove
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
get_opcode_stats(u32 opCount, OpCode *opCodes)
{
    OpCodeStats result = {0};
    
    result.maxSelect = Select_Count - 1;
    result.selectBits = log2_up(result.maxSelect);
    result.maxAluOp = Alu_Count - 1;
    result.aluOpBits = log2_up(result.maxAluOp);
    result.maxImmediate = opc_max_immediate(opCount, opCodes);
    result.immediateBits = log2_up(result.maxImmediate);
    result.maxAddress = opc_max_address(opCount, opCodes);
    result.addressBits = log2_up(result.maxAddress);
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
build_expression(Expression *expr, OpCodeBuilder *builder)
{
    // TODO(michiel): First ssa and constant folding e.d.
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
        build_expression(expr->leftExpr, builder);
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

internal void
print_expr(Expr *expr)
{
    switch (expr->kind)
    {
        case Expr_Paren:
        {
            fprintf(stdout, "(");
            print_expr(expr->paren.expr);
            fprintf(stdout, ")");
        } break;
        
        case Expr_Int:
        {
            fprintf(stdout, "%ld", expr->intConst);
        } break;
        
        case Expr_Id:
        {
            fprintf(stdout, "%.*s", expr->name.size, expr->name.data);
        } break;
        
        case Expr_Unary:
        {
            fprintf(stdout, "[unary ");
            if (expr->unary.op == TOKEN_INC)
            {
                fprintf(stdout, "++");
            }
            else if (expr->unary.op == TOKEN_DEC)
            {
                fprintf(stdout, "--");
            }
            else
            {
                fprintf(stdout, "%c", expr->unary.op);
            }
            print_expr(expr->unary.expr);
            fprintf(stdout, "]");
        } break;
        
        case Expr_Binary:
        {
            fprintf(stdout, "[binary ");
            print_expr(expr->binary.left);
            if (expr->binary.op == TOKEN_SLL)
            {
                fprintf(stdout, "<<");
            }
            else if (expr->binary.op == TOKEN_SRA)
            {
                fprintf(stdout, ">>");
            }
            else if (expr->binary.op == TOKEN_SRL)
            {
                fprintf(stdout, ">>>");
            }
            else
            {
                fprintf(stdout, "%c", expr->binary.op);
            }
            print_expr(expr->binary.right);
            fprintf(stdout, "]");
        } break;
        
        INVALID_DEFAULT_CASE;
    }
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
            
            for (u32 stmtIdx = 0; stmtIdx < stmts->stmtCount; ++stmtIdx)
            {
                Stmt *stmt = stmts->stmts[stmtIdx];
                fprintf(stdout, "Stmt %d:\n", stmtIdx);
                if (stmt->kind == Stmt_Assign)
                {
                    fprintf(stdout, "  assign: ");
                    print_expr(stmt->assign.left);
                    fprintf(stdout, " = ");
                    combine_const(&astOptimizer, stmt->assign.right);
                    print_expr(stmt->assign.right);
                }
                else
                {
                    i_expect(stmt->kind == Stmt_Hint);
                    fprintf(stdout, "  hint: ");
                    print_expr(stmt->expr);
                }
                fprintf(stdout, "\n");
            }
            graph_ast(stmts, "ast.dot");
            
            u32 trimmed = 0;
            for (Expr *fre = astOptimizer.exprFreeList;
                 fre;
                 fre = fre->nextFree)
            {
                i_expect(fre->kind == Expr_None);
                ++trimmed;
            }
            fprintf(stdout, "Trimmed %d expressions\n", trimmed);
            return 0;
            
        Program *program = parse(tokens);
        OpCode *opCodes = 0;
        
        b32 synced = false;
        
        // print_tokens(tokens);
        print_parsed_program(outputStream, program);
            graph_program(program, "program.dot");
            
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

        OpCodeStats opCodeStats = get_opcode_stats(buf_len(opCodes), opCodes);
        fprintf(stdout, "Stats:\n");
        fprintf(stdout, "  SEL: Max = %u, Bits = %u\n", opCodeStats.maxSelect, opCodeStats.selectBits);
        fprintf(stdout, "  ALU: Max = %u, Bits = %u\n", opCodeStats.maxAluOp, opCodeStats.aluOpBits);
        fprintf(stdout, "  IMM: Max = %u, Bits = %u\n", opCodeStats.maxImmediate, opCodeStats.immediateBits);
        fprintf(stdout, "  ADR: Max = %u, Bits = %u\n", opCodeStats.maxAddress, opCodeStats.addressBits);
            
            u32 inputs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
            simulate(&opCodeStats, buf_len(opCodes), opCodes, array_count(inputs), inputs,
                     100);
            
#if 1
            FileStream printStream = {0};
            printStream.file = fopen("opcodes.list", "wb");
            printStream.verbose = true;
        for (u32 opIdx = 0; opIdx < buf_len(opCodes); ++opIdx)
        {
            print_opcode(printStream, &opCodeStats, opCodes + opIdx);
        }
        #endif
            test_graph("testing.dot", gRegisterCount, buf_len(opCodes), opCodes, false);
            test_graph("opcodes.dot", gRegisterCount, buf_len(opCodes), opCodes, true);
        //save_graph("opcodes.dot", gRegisterCount, buf_len(opCodes), opCodes);
        
        opCodeStats.synced = synced;
        opCodeStats.bitWidth = 8;
        
        FileStream opCodeStream = {0};
        opCodeStream.file = fopen("gen_opcodes.vhd", "wb");
        generate_opcode_vhdl(&opCodeStats, opCodes, opCodeStream);
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
