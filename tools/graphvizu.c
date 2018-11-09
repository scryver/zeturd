#define LAYER_HAS_MEMA 0x001
#define LAYER_HAS_MEMB 0x002
#define LAYER_HAS_IMM  0x004
#define LAYER_HAS_IOIN 0x008
#define LAYER_HAS_IOUT 0x010
#define LAYER_HAS_ALU  0x020
#define LAYER_HAS_MEMI 0x100
#define LAYER_HAS_MEMW 0x200

internal void
save_graph(char *fileName, u32 registers, u32 opCodeCount, OpCodeBuild *opCodes)
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
    OpCodeBuild opCode = opCodes[opIdx];
    
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
test_graph(char *fileName, u32 registers, u32 opCodeCount, OpCodeBuild *opCodes, b32 minimize)
{
    FileStream graphicStream = {0};
    graphicStream.file = fopen(fileName, "wb");
    
    fprintf(graphicStream.file, "digraph testing {\n");
    fprintf(graphicStream.file, "    rankdir=LR;\n\n");

#if 0    
    fprintf(graphicStream.file, "    imm0 [shape=record, label=\"0\"];\n");
    fprintf(graphicStream.file, "    ioIn0 [shape=record, label=\"IO In\"];\n");
#endif
    fprintf(graphicStream.file,"    mem0 [shape=record, label=\"{{mem[0]|mem[0]}|{<a>a|<b>b}}\"];\n");
    fprintf(graphicStream.file,"    alu0 [shape=record, label=\"{alu|<out>out}\"];\n");
    
    for (u32 opIdx = 0; opIdx < opCodeCount; ++opIdx)
    {
    OpCodeBuild opCode = opCodes[opIdx];
    
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
                "    mem%d [shape=record, label=\"{<in>in|{mem[%d]|mem[%d]}|{<a>a|<b>b}}\"];\n",
                graphIdx, opCode.memoryAddrA, opCode.memoryAddrB);
        
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
