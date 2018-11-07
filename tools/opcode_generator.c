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
};

enum AluOp
{
    Alu_Noop,
    Alu_And,
    Alu_Or,
    Alu_Add,
};

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

#define OPC_IMM_BITS    32
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
push_expression(Expression *expr, OpCode **opCodes)
{
    OpCode loadOp = {0};
    OpCode op = {0};
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
                op.selectAluA = Select_IO;
            }
            else
            {
                loadOp.memoryReadA = true;
                loadOp.memoryAddrA = get_register_location(idString);
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
                    loadOp.useMemoryB = true;
                    loadOp.memoryReadB = true;
                    loadOp.memoryAddrB = get_register_location(idString);
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
    
    if (loadOp.totalOp)
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
    
    i_expect(assign->id->name.size);
    String idString = assign->id->name;
    
    if (strings_are_equal(idString, str_internalize_cstring("IO")))
    {
        store.selectIO = Select_Alu;
    }
    else
    {
        store.selectMem = Select_Alu;
    store.memoryWrite = true;
        store.memoryAddrA = get_register_location(idString);
        }
    
    buf_push(*opCodes, store);
}

internal void
optimize_assignments(OpCode **opCodes)
{
    // NOTE(michiel): Removes alu no-ops to assignment selection
    u32 opCodeCount = buf_len(*opCodes);
    for (u32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCode cur = (*opCodes)[opIdx];
        OpCode prev = (*opCodes)[opIdx - 1];
        
        if ((cur.totalOp & OPC_SEL_MASK) &&
            !(cur.totalOp & ~OPC_SEL_MASK))
        {
            // NOTE(michiel): Only selection is set for cur
            if ((prev.selectAluA) &&
                !(prev.totalOp & ~OPC_SEL_MASK))
            {
                // NOTE(michiel): Only alu A selection with a alu no-op for prev
                
                if (cur.selectAluA)
                {
                }
                else if (cur.selectAluB)
                {
                    prev.selectAluB = prev.selectAluA;
                }
                else if (cur.selectMem)
                {
                    prev.selectMem = prev.selectAluA;
                }
                else 
                {
                    i_expect(cur.selectIO);
                    prev.selectIO = prev.selectAluA;
                }
                prev.selectAluA = Select_Zero;
                
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
    for (u32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCode cur = (*opCodes)[opIdx];
        OpCode prev = (*opCodes)[opIdx - 1];
        
        if (cur.memoryReadA &&
            !cur.memoryReadB &&
            !cur.memoryWrite &&
            (!(cur.totalOp & OPC_SEL_MASK) ||
             ((cur.selectAluA == Select_Alu) &&
              !cur.selectAluB && !cur.selectMem && !cur.selectIO)) &&
            (cur.aluOperation == Alu_Noop))
        {
            // NOTE(michiel): Read setup for mem A
            
            if (!(prev.totalOp & OPC_MEMCTL_MASK))
            {
                // NOTE(michiel): If previous doesn't use memory
                prev.memoryReadA = cur.memoryReadA;
                prev.memoryAddrA = cur.memoryAddrA;
                
                for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                {
                    (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                }
                --buf_len_(*opCodes);
                --opCodeCount;
                --opIdx;
                (*opCodes)[opIdx] = prev;
            }
            else if (prev.memoryWrite &&
                     !prev.memoryReadA &&
                     !prev.memoryReadB &&
                     (prev.memoryAddrA == cur.memoryAddrA))
            {
                // NOTE(michiel): Previous was write to the same memory address
                
                if (opIdx < (opCodeCount - 1))
                {
                    OpCode next = (*opCodes)[opIdx + 1];
                    
                    if ((next.selectAluA == Select_MemoryA) ||
                        (next.selectAluB == Select_MemoryA) ||
                        (next.selectMem == Select_MemoryA) ||
                        (next.selectIO == Select_MemoryA))
                    {
                    if (next.selectAluA == Select_MemoryA)
                    {
                        next.selectAluA = Select_Alu;
                        }
                        else if (next.selectAluB == Select_MemoryA)
                        {
                            next.selectAluB = Select_Alu;
                            }
                        else if (next.selectMem == Select_MemoryA)
                        {
                            next.selectMem = Select_Alu;
                        }
                        else
                        {
                            i_expect(next.selectIO == Select_MemoryA);
                            next.selectIO = Select_Alu;
                        }
                        
                        prev.selectAluA = Select_Alu;
                        
                        for (u32 popIdx = opIdx; popIdx < opCodeCount - 1; ++popIdx)
                        {
                            (*opCodes)[popIdx] = (*opCodes)[popIdx + 1];
                        }
                        --buf_len_(*opCodes);
                        --opCodeCount;
                        --opIdx;
                        (*opCodes)[opIdx] = prev;
                        (*opCodes)[opIdx + 1] = next;
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
    for (u32 opIdx = opCodeCount - 1; opIdx > 0; --opIdx)
    {
        OpCode cur = (*opCodes)[opIdx];
        OpCode prev = (*opCodes)[opIdx - 1];
        
        if (cur.aluOperation &&
            ((cur.selectAluA == Select_Alu) ||
             (cur.selectAluB == Select_Alu)) &&
            ((cur.selectAluA == Select_Immediate) ||
             (cur.selectAluB == Select_Immediate)) &&
            !(cur.totalOp & OPC_MEMCTL_MASK))
        {
            // NOTE(michiel): Alu operation with alu out as one of the inputs and no memory mods
            
            if ((prev.aluOperation == Alu_Noop) &&
                !prev.memoryReadA &&
                !prev.memoryReadB)
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
        
        // print_tokens(tokens);
        print_parsed_program(outputStream, program);
        
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
                push_expression(statement->expr, &opCodes);
            }
        }
        
        //fprintf(stdout, "Pre Opcodes: %u\n", buf_len(opCodes));
        optimize_assignments(&opCodes);
        optimize_read_setups(&opCodes);
        optimize_combine_write(&opCodes);
        //fprintf(stdout, "Post Opcodes: %u\n", buf_len(opCodes));
        
#if 0
        for (u32 opIdx = 0; opIdx < buf_len(opCodes); ++opIdx)
        {
            OpCode opCode = opCodes[opIdx];
            print_opcode(opCode, true);
        }
#else
        FileStream opCodeStream = {0};
        opCodeStream.file = fopen("gen_opcodes.vhd", "wb");
        generate_opcode_vhdl(opCodes, opCodeStream);
        fclose(opCodeStream.file);
        
        opCodeStream.file = fopen("gen_controller.vhd", "wb");
        generate_controller(8, buf_len(opCodes), opCodeStream);
        fclose(opCodeStream.file);
        
        opCodeStream.file = fopen("gen_constants.vhd", "wb");
        generate_constants(opCodeStream);
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