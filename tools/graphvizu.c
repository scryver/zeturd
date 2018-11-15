typedef struct GraphState
{
    FileStream output;
    Map *nodes;
    u32 index;
    u32 opIndex;
    u32 indent;
} GraphState;

internal void
print_graph_line(GraphState *state, char *fmt, ...)
{
    if (state->indent)
    {
    char indent[128];
    for (u32 i = 0; i < state->indent; ++i)
    {
        indent[i * 4 + 0] = ' ';
        indent[i * 4 + 1] = ' ';
        indent[i * 4 + 2] = ' ';
        indent[i * 4 + 3] = ' ';
        indent[i * 4 + 4] = 0;
    }
    fprintf(state->output.file, "%s", indent);
    }
    
    va_list args;
    va_start(args, fmt);
    vfprintf(state->output.file, fmt, args);
    va_end(args);
    fputc('\n', state->output.file);
}

internal String
graph_variable(GraphState *state, Variable *var)
{
    String result = {0};
    String label = {0};
    char buf[128];
    if (var->kind == VARIABLE_IDENTIFIER)
    {
        snprintf(buf, sizeof(buf), "%.*s_%d", var->id->name.size, var->id->name.data,
                 state->index);
         label = var->id->name;
        result = str_internalize_cstring(buf);
        }
    else
    {
        i_expect(var->kind == VARIABLE_CONSTANT);
        snprintf(buf, sizeof(buf), "%d", var->constant->value);
        label = str_internalize_cstring(buf);
        snprintf(buf, sizeof(buf), "const_%d_%d", var->constant->value, state->index);
        result = str_internalize_cstring(buf);
    }
    print_graph_line(state, "%.*s [shape=record, label=\"%.*s\"];",
            result.size, result.data, label.size, label.data);
    return result;
}

internal String
graph_expression(GraphState *state, Expression *expr)
{
    String result = {0};
    String left = {0};
    String right = {0};
    
    if (expr->leftKind == EXPRESSION_VAR)
    {
        left = graph_variable(state, expr->left);
    }
    else
    {
        i_expect(expr->leftKind == EXPRESSION_EXPR);
        left = graph_expression(state, expr->leftExpr);
    }
    
    if (expr->op != EXPR_OP_NOP)
    {
        if (expr->rightKind == EXPRESSION_VAR)
        {
            right = graph_variable(state, expr->right);
        }
        else
        {
            i_expect(expr->rightKind == EXPRESSION_EXPR);
            right = graph_expression(state, expr->rightExpr);
        }
    }

#if 0    
    u64 hash = hash_bytes(left.data, left.size);
    u64 key = hash ? hash : 1;
    String *leftM = map_get_from_u64(state->nodes, key);
    #endif

    
    if (expr->op == EXPR_OP_NOP)
    {
        result = left;
    }
    else
    {
        i_expect(left.size);
        i_expect(right.size);
        char buf[128];
        
        char *op = "";
        char *opLabel = "";
        switch (expr->op)
        {
            case EXPR_OP_AND: { op = "AND"; opLabel = "&"; } break;
            case EXPR_OP_OR: { op = "OR"; opLabel = "|"; } break;
            case EXPR_OP_ADD: { op = "ADD"; opLabel = "+"; } break;
            INVALID_DEFAULT_CASE;
        }
        
        snprintf(buf, sizeof(buf), "%s_%d", op, state->opIndex++);
        result = str_internalize_cstring(buf);
        
        print_graph_line(state, "%.*s [label=\"%s\"];", result.size, result.data, opLabel);
        print_graph_line(state, "%.*s -> %.*s;", left.size, left.data, 
                         result.size, result.data);
        print_graph_line(state, "%.*s -> %.*s;", right.size, right.data, 
                         result.size, result.data);
    }
    
    return result;
}

internal String
graph_assignment(GraphState *state, Assignment *assign)
{
    String name = assign->id->name;
    char buf[128];
    snprintf(buf, sizeof(buf), "%.*s_%d", name.size, name.data, state->index);
    String result = str_internalize_cstring(buf);
    String exprLabel = graph_expression(state, assign->expr);
    
    print_graph_line(state, "%.*s [shape=record, label=\"%.*s\"];",
                     result.size, result.data, assign->id->name.size, assign->id->name.data);
    print_graph_line(state, "%.*s -> %.*s;\n", exprLabel.size, exprLabel.data,
                     result.size, result.data);
    
    return result;
}

internal String
graph_statement(GraphState *state, Statement *statement)
{
    String result = {0};
    if (statement->kind == STATEMENT_ASSIGN)
    {
        result = graph_assignment(state, statement->assign);
    }
    else
    {
        i_expect(statement->kind == STATEMENT_EXPR);
        //result = graph_expression(state, statement->expr);
    }
    return result;
}

internal void
graph_program(Program *program, char *fileName)
{
    GraphState state = {0};
    state.output.file = fopen(fileName, "wb");
    
    print_graph_line(&state, "digraph testing {");
    ++state.indent;
    print_graph_line(&state, "compound=true;");
    print_graph_line(&state, "graph [nodesep=0];");
    print_graph_line(&state, "node [peripheries=1];");
    //fprintf(state.output.file, "    rankdir=LR;\n ranksep=\"1\";\n\n");
    
    String prevResult = {0};
    
    Map printedNodes = {0};
    state.nodes = &printedNodes;
    
    char buf[128];
        for (u32 statementIndex = 0;
         statementIndex < program->nrStatements;
         ++statementIndex)
    {
        snprintf(buf, sizeof(buf), "stmt%d", statementIndex);
        String next = str_internalize_cstring(buf);
        Statement *statement = program->statements + statementIndex;
        print_graph_line(&state, "subgraph cluster_%d {", statementIndex);
        ++state.indent;
        state.index = statementIndex;
           graph_statement(&state, statement);
        print_graph_line(&state, "label = \"Statement %d\";", statementIndex);
        print_graph_line(&state, "%.*s [shape=point, peripheries=0, style=invis, width=0, height=0, penwidth=0];", 
                         next.size, next.data);
        --state.indent;
        print_graph_line(&state, "}");
        if (prevResult.size)
        {
            print_graph_line(&state, "%.*s -> %.*s [ltail=cluster_%d, lhead=cluster_%d];",
                             prevResult.size, prevResult.data, next.size, next.data,
                             statementIndex - 1, statementIndex);
        }
        prevResult = next;
    }
    --state.indent;
    print_graph_line(&state, "}\n");
}

#define LAYER_HAS_MEMA 0x001
#define LAYER_HAS_MEMB 0x002
#define LAYER_HAS_IMM  0x004
#define LAYER_HAS_IOIN 0x008
#define LAYER_HAS_IOUT 0x010
#define LAYER_HAS_ALU  0x020
#define LAYER_HAS_MEMI 0x100
#define LAYER_HAS_MEMW 0x200

internal void
save_graph(char *fileName, u32 registers, u32 opCodeCount, OpCode *opCodes)
{
FileStream graphicStream = {0};
graphicStream.file = fopen(fileName, "wb");

fprintf(graphicStream.file, "digraph opcodes {\n");
fprintf(graphicStream.file, "  ranksep=.75;\n");

for (u32 regIdx = 0; regIdx < registers; ++regIdx)
{
    fprintf(graphicStream.file, "    mem%d [shape=record, label=\"{ <in>in | mem[%d] | {<a>a|<b>b} }\"];\n", regIdx, regIdx);
}

u32 nextLayerHas = 0;
for (u32 opIdx = 0; opIdx < opCodeCount; ++opIdx)
{
    OpCode opCode = opCodes[opIdx];
        
        fprintf(graphicStream.file, "  subgraph cluster%d {\n", opIdx);
    
    u32 layerHas = nextLayerHas;
    nextLayerHas = 0;
    
    switch (opCode.selectIO)
    {
        case Select_Zero: {
            //fprintf(graphicStream.file, "    ZERO%03d -> ioOut%03d\n",
            //opIdx, opIdx + 1);
        } break;
        case Select_MemoryA: {
            fprintf(graphicStream.file, "    memA%03d -> ioOut%03d\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_MEMA | LAYER_HAS_IOUT;
        } break;
        case Select_MemoryB: {
            fprintf(graphicStream.file, "    memB%03d -> ioOut%03d\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_MEMB | LAYER_HAS_IOUT;
        } break;
        case Select_Immediate: {
            fprintf(graphicStream.file, "    imm%03d -> ioOut%03d\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_IMM | LAYER_HAS_IOUT;
        } break;
        case Select_IO: {
            fprintf(graphicStream.file, "    ioIn%03d -> ioOut%03d\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_IOIN | LAYER_HAS_IOUT;
        } break;
        case Select_Alu: {
            fprintf(graphicStream.file, "    alu%03d:out -> ioOut%03d\n",
                    opIdx - 1, opIdx);
            layerHas |= LAYER_HAS_IOUT;
        } break;
        INVALID_DEFAULT_CASE;
    }
    switch (opCode.selectMem)
    {
        case Select_Zero: {
            //fprintf(graphicStream.file, "    ZERO%03d -> memIn%03d\n",
            //opIdx, opIdx);
        } break;
        case Select_MemoryA: {
            fprintf(graphicStream.file, "    memA%03d -> memIn%03d\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_MEMA | LAYER_HAS_MEMI;
        } break;
        case Select_MemoryB: {
            fprintf(graphicStream.file, "    memB%03d -> memIn%03d\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_MEMB | LAYER_HAS_MEMI;
        } break;
        case Select_Immediate: {
            fprintf(graphicStream.file, "    imm%03d -> memIn%03d\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_IMM | LAYER_HAS_MEMI;
        } break;
        case Select_IO: {
            fprintf(graphicStream.file, "    ioIn%03d -> memIn%03d\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_IOIN | LAYER_HAS_MEMI;
        } break;
        case Select_Alu: {
            fprintf(graphicStream.file, "    alu%03d:out -> memIn%03d\n",
                    opIdx - 1, opIdx);
            layerHas |= LAYER_HAS_MEMI;
        } break;
        INVALID_DEFAULT_CASE;
    }
    switch (opCode.selectAluA)
    {
        case Select_Zero: {
            //fprintf(graphicStream.file, "    ZERO%03d -> aluA%03d\n",
            //opIdx - 1, opIdx);
        } break;
        case Select_MemoryA: {
            fprintf(graphicStream.file, "    memA%03d -> alu%03d:a\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_MEMA;
        } break;
        case Select_MemoryB: {
            fprintf(graphicStream.file, "    memB%03d -> alu%03d:a\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_MEMB;
        } break;
        case Select_Immediate: {
            fprintf(graphicStream.file, "    imm%03d -> alu%03d:a\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_IMM;
        } break;
        case Select_IO: {
            fprintf(graphicStream.file, "    ioIn%03d -> alu%03d:a\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_IOIN;
        } break;
        case Select_Alu: {
            fprintf(graphicStream.file, "    alu%03d:out -> alu%03d:a\n",
                    opIdx - 1, opIdx);
        } break;
        INVALID_DEFAULT_CASE;
    }
    switch (opCode.selectAluB)
    {
        case Select_Zero: {
            //fprintf(graphicStream.file, "    ZERO%03d -> aluB%03d\n",
            //opIdx - 1, opIdx);
        } break;
        case Select_MemoryA: {
            fprintf(graphicStream.file, "    memA%03d -> alu%03d:b\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_MEMA;
        } break;
        case Select_MemoryB: {
            fprintf(graphicStream.file, "    memB%03d -> alu%03d:b\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_MEMB;
        } break;
        case Select_Immediate: {
            fprintf(graphicStream.file, "    imm%03d -> alu%03d:b\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_IMM;
        } break;
        case Select_IO: {
            fprintf(graphicStream.file, "    ioIn%03d -> alu%03d:b\n",
                    opIdx, opIdx);
            layerHas |= LAYER_HAS_IOIN;
        } break;
        case Select_Alu: {
            fprintf(graphicStream.file, "    alu%03d:out -> alu%03d:b\n",
                    opIdx - 1, opIdx);
        } break;
        INVALID_DEFAULT_CASE;
    }
    
    if (opCode.selectAluA || opCode.selectAluB || (layerHas & LAYER_HAS_ALU))
    {
        char *aluOp = "";
        switch (opCode.aluOperation)
        {
            case Alu_Noop: { aluOp = ""; } break;
            case Alu_And: { aluOp = "and"; } break;
            case Alu_Or: { aluOp = "or"; } break;
            case Alu_Add: { aluOp = "add"; } break;
            INVALID_DEFAULT_CASE;
        }
        fprintf(graphicStream.file, "    alu%03d [shape=record, label=\"{ {<a>a|<b>b} | %s[%03d] | <out>out}\"];\n", opIdx, aluOp, opIdx);
        if (opCode.selectAluA || opCode.selectAluB)
        {
            nextLayerHas |= LAYER_HAS_ALU;
        }
        layerHas |= LAYER_HAS_ALU;
    }
    
    if (opCode.memoryReadA)
    {
        fprintf(graphicStream.file, "    mem%d:a -> loadA%03d -> memA%03d\n",
                opCode.memoryAddrA, opIdx, opIdx + 1);
        nextLayerHas |= LAYER_HAS_MEMA;
    }
    if (opCode.memoryReadB)
    {
        fprintf(graphicStream.file, "    mem%d:b -> loadB%03d -> memB%03d\n",
                opCode.memoryAddrB, opIdx, opIdx + 1);
        nextLayerHas |= LAYER_HAS_MEMB;
    }
    if (opCode.memoryWrite)
    {
        fprintf(graphicStream.file, "    memIn%03d -> write%03d -> mem%d:in\n",
                opIdx, opIdx + 1, opCode.memoryAddrA);
        nextLayerHas |= LAYER_HAS_MEMW;
    }
    
    
    fprintf(graphicStream.file, "      { rank = same; ");
    if (layerHas & LAYER_HAS_MEMA)
    {
        fprintf(graphicStream.file, "memA%03d; ", opIdx);
    }
    if (layerHas & LAYER_HAS_MEMB)
    {
        fprintf(graphicStream.file, "memB%03d; ", opIdx);
    }
    if (layerHas & LAYER_HAS_MEMI)
    {
        fprintf(graphicStream.file, "memIn%03d; ", opIdx);
    }
    
    if (nextLayerHas & LAYER_HAS_MEMA)
    {
        fprintf(graphicStream.file, "loadA%03d; ", opIdx);
    }
    if (nextLayerHas & LAYER_HAS_MEMB)
    {
        fprintf(graphicStream.file, "loadB%03d; ", opIdx);
    }
    if (layerHas & LAYER_HAS_MEMW)
    {
        fprintf(graphicStream.file, "write%03d; ", opIdx);
    }
    if (layerHas & LAYER_HAS_IMM)
    {
        fprintf(graphicStream.file, "imm%03d; ", opIdx);
    }
    if (layerHas & LAYER_HAS_IOIN)
    {
        fprintf(graphicStream.file, "ioIn%03d; ", opIdx);
    }
    if (layerHas & LAYER_HAS_IOUT)
    {
        fprintf(graphicStream.file, "ioOut%03d; ", opIdx);
    }
    if (layerHas & LAYER_HAS_ALU)
    {
        //fprintf(graphicStream.file, "alu%03d; ", opIdx);
    }
    fprintf(graphicStream.file, "};\n");
        
        fprintf(graphicStream.file, "  }\n");
}
fprintf(graphicStream.file, "}\n");

fclose(graphicStream.file);
}

internal void
print_selection(FileStream stream, char *name, u32 index, enum Selection select)
{
    char *switchIn = "<0>0|<a>A|<b>B|<imm>Imm|<io>IO|<alu>Alu";
    fprintf(stream.file, "    sel%s%d [shape=record, label=\"%s\"];\n", name, index, switchIn);
    fprintf(stream.file, "    out%s%d [shape=record, label=\"%s Out\"];\n", name, index, name);
    
    fprintf(stream.file, "    sel%s%d:0 -> out%s%d%s;\n", name, index, name, index,
            (select == Select_Zero) ? "" : " [style=dotted]");
    fprintf(stream.file, "    sel%s%d:a -> out%s%d%s;\n", name, index, name, index,
            (select == Select_MemoryA) ? "" : " [style=dotted]");
    fprintf(stream.file, "    sel%s%d:b -> out%s%d%s;\n", name, index, name, index,
            (select == Select_MemoryB) ? "" : " [style=dotted]");
    fprintf(stream.file, "    sel%s%d:imm -> out%s%d%s;\n", name, index, name, index,
            (select == Select_Immediate) ? "" : " [style=dotted]");
    fprintf(stream.file, "    sel%s%d:io -> out%s%d%s;\n", name, index, name, index,
            (select == Select_IO) ? "" : " [style=dotted]");
    fprintf(stream.file, "    sel%s%d:alu -> out%s%d%s;\n", name, index, name, index,
            (select == Select_Alu) ? "" : " [style=dotted]");
    
    fprintf(stream.file, "    mem%d:a:e -> sel%s%d:a%s;\n", index - 1, name, index,
            (select == Select_MemoryA) ? "" : " [style=dotted]");
    fprintf(stream.file, "    mem%d:b:e -> sel%s%d:b%s;\n", index - 1, name, index,
            (select == Select_MemoryB) ? "" : " [style=dotted]");
    fprintf(stream.file, "    imm%d:e -> sel%s%d:imm%s;\n", index, name, index,
            (select == Select_Immediate) ? "" : " [style=dotted]");
    fprintf(stream.file, "    ioIn%d:e -> sel%s%d:io%s;\n", index, name, index,
            (select == Select_IO) ? "" : " [style=dotted]");
    fprintf(stream.file, "    alu%d:out -> sel%s%d:alu%s;\n", index - 1, name, index,
            (select == Select_Alu) ? "" : " [style=dotted]");
}

internal void
test_graph(char *fileName, u32 registers, u32 opCodeCount, OpCode *opCodes, b32 minimize)
{
    FileStream graphicStream = {0};
    graphicStream.file = fopen(fileName, "wb");
    
    fprintf(graphicStream.file, "digraph testing {\n");
    fprintf(graphicStream.file, "    rankdir=LR;\n    ranksep=\"1\";\n\n");

#if 0    
    fprintf(graphicStream.file, "    imm0 [shape=record, label=\"0\"];\n");
    fprintf(graphicStream.file, "    ioIn0 [shape=record, label=\"IO In\"];\n");
#endif
    fprintf(graphicStream.file,"    mem0 [shape=record, label=\"{{mem[0]|mem[0]}|{<a>a|<b>b}}\"];\n");
    fprintf(graphicStream.file,"    alu0 [shape=record, label=\"{alu|<out>out}\"];\n");
    
    for (u32 opIdx = 0; opIdx < opCodeCount; ++opIdx)
    {
    OpCode opCode = opCodes[opIdx];
        
    char *aluOp = "";
    switch (opCode.aluOperation)
    {
        case Alu_Noop: { aluOp = "thru"; } break;
        case Alu_And: { aluOp = "and"; } break;
        case Alu_Or: { aluOp = "or"; } break;
        case Alu_Add: { aluOp = "add"; } break;
        INVALID_DEFAULT_CASE;
    }
        
        u32 graphIdx = opIdx + 1;

        fprintf(graphicStream.file,
                "    imm%d [shape=record, label=\"%d: %d\"];\n", graphIdx, graphIdx, opCode.immediate);
        fprintf(graphicStream.file,
                "    ioIn%d [shape=record, label=\"%d: IO In\"];\n", graphIdx, graphIdx);
            
        fprintf(graphicStream.file,
                "    mem%d [shape=record, "
                "label=\"{<in>in|{mem[%d]%s|mem[%d]%s}|{<a>a|<b>b}}\"];\n", graphIdx, 
                opCode.memoryAddrA, opCode.memoryReadA ? " r" : "",
                opCode.memoryAddrB, opCode.memoryReadB ? " r" : "");
        
    fprintf(graphicStream.file,
            "    alu%d [shape=record, label=\"{{<a>a|<b>b}|%s|<out>out}\"];\n",
            graphIdx, aluOp);
        
        if (!minimize || opCode.selectIO)
        {
        print_selection(graphicStream, "IO", graphIdx, opCode.selectIO);
        }
        if (!minimize || opCode.memoryWrite)
        {
        print_selection(graphicStream, "Mem", graphIdx, opCode.selectMem);
            
            fprintf(graphicStream.file, "    outMem%d:e -> mem%d:in%s;\n", graphIdx, graphIdx,
                    (opCode.memoryWrite) ? "" : " [style=dotted]");
        }
        
        if (!minimize || (opCode.aluOperation != Alu_Noop) || (opCode.selectAluA))
        {
        print_selection(graphicStream, "AluA", graphIdx, opCode.selectAluA);
        fprintf(graphicStream.file, "    outAluA%d:e -> alu%d:a:w;\n", graphIdx, graphIdx);
        }
        if (!minimize || (opCode.aluOperation != Alu_Noop))
        {
        print_selection(graphicStream, "AluB", graphIdx, opCode.selectAluB);
        fprintf(graphicStream.file, "    outAluB%d:e -> alu%d:b:w;\n", graphIdx, graphIdx);
    }
    }
    
    fprintf(graphicStream.file, "}\n");
    
    fclose(graphicStream.file);
}
