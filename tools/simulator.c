#define MAX_REG 2048
global u32 gSimRegisters[MAX_REG];
    typedef struct SimState
{
    u32 memAddrA;
    u32 memAddrB;
    u32 aluOut;
    u32 ioIn;
    u32 ioOut;
} SimState;

internal void
simulate(OpCodeStats *stats, u32 opCodeCount, OpCode *opCodes, u32 inputCount, u32 *inputs,
         u32 clockTicks)
{
    i_expect(inputCount);
    SimState state = {0};
    
    u32 inputIndex = 0;
    state.ioIn = inputs[inputIndex];
    inputIndex = (inputIndex + 1) % inputCount;
    
    fprintf(stdout, "Tick %3d: ", 0);
    fprintf(stdout, "State: AddrA(%2d), AddrB(%2d), Alu(%2d), ioIn(%2d), ioOut(%2d) | ", state.memAddrA, state.memAddrB,
            state.aluOut, state.ioIn, state.ioOut);
    fprintf(stdout, "Mem  : 0(%2d), 1(%2d), 2(%2d), 3(%2d), 4(%2d), 5(%2d), 6(%2d)\n",
            gSimRegisters[0], gSimRegisters[1], gSimRegisters[2], gSimRegisters[3], gSimRegisters[4], 
            gSimRegisters[5], gSimRegisters[6]);
    
    for (u32 tick = 0; (tick < clockTicks) && (tick < opCodeCount); ++tick)
    {
        OpCode *opCode = opCodes + tick;
        SimState nextState = {0};
        
        u32 aluA = 0;
        u32 aluB = 0;
        switch (opCode->selectAluA)
        {
            case Select_Zero: { aluA = 0; } break;
            case Select_MemoryA: { aluA = gSimRegisters[state.memAddrA]; } break;
            case Select_MemoryB: { aluA = gSimRegisters[state.memAddrB]; } break;
             case Select_Immediate: { aluA = opCode->immediate; } break;
            case Select_IO: { aluA = state.ioIn; } break;
            case Select_Alu: { aluA = state.aluOut; } break;
            INVALID_DEFAULT_CASE;
        }
        switch (opCode->selectAluB)
        {
            case Select_Zero: { aluB = 0; } break;
            case Select_MemoryA: { aluB = gSimRegisters[state.memAddrA]; } break;
            case Select_MemoryB: { aluB = gSimRegisters[state.memAddrB]; } break;
            case Select_Immediate: { aluB = opCode->immediate; } break;
            case Select_IO: { aluB = state.ioIn; } break;
            case Select_Alu: { aluB = state.aluOut; } break;
            INVALID_DEFAULT_CASE;
        }
        
        switch (opCode->aluOperation)
        {
            case Alu_Noop:
            {
                nextState.aluOut = aluA;
            } break;
            
            case Alu_And:
            {
                nextState.aluOut = aluA & aluB;
            } break;
            
            case Alu_Or:
            {
                nextState.aluOut = aluA | aluB;
            } break;
            
            case Alu_Add:
            {
                nextState.aluOut = aluA + aluB;
            } break;
            
            INVALID_DEFAULT_CASE;
        }
        
            switch (opCode->selectIO)
            {
                case Select_Zero: { } break;
            case Select_MemoryA: { nextState.ioOut = gSimRegisters[state.memAddrA]; } break;
            case Select_MemoryB: { nextState.ioOut = gSimRegisters[state.memAddrB]; } break;
            case Select_Immediate: { nextState.ioOut = opCode->immediate; } break;
            case Select_IO: { nextState.ioOut = state.ioIn; } break;
            case Select_Alu: { nextState.ioOut = state.aluOut; } break;
                INVALID_DEFAULT_CASE;
            }
        
        if (opCode->memoryWrite)
        {
            switch (opCode->selectMem)
            {
                case Select_Zero     : { gSimRegisters[opCode->memoryAddrA] = 0; } break;
                case Select_MemoryA  : { gSimRegisters[opCode->memoryAddrA] = gSimRegisters[state.memAddrA]; } break;
                case Select_MemoryB  : { gSimRegisters[opCode->memoryAddrA] = gSimRegisters[state.memAddrB]; } break;
                case Select_Immediate: { gSimRegisters[opCode->memoryAddrA] = opCode->immediate; } break;
                case Select_IO       : { gSimRegisters[opCode->memoryAddrA] = state.ioIn; } break;
                case Select_Alu      : { gSimRegisters[opCode->memoryAddrA] = state.aluOut; } break;
                
                INVALID_DEFAULT_CASE;
            }
        }
        
        if (opCode->memoryReadA)
        {
            nextState.memAddrA = opCode->memoryAddrA;
        }
        if (opCode->memoryReadB)
        {
            nextState.memAddrB = opCode->memoryAddrB;
        }
        
        nextState.ioIn = inputs[inputIndex];
        state = nextState;
        inputIndex = (inputIndex + 1) % inputCount;
        
        fprintf(stdout, "Tick %3d: ", tick + 1);
        fprintf(stdout, "State: AddrA(%2d), AddrB(%2d), Alu(%2d), ioIn(%2d), ioOut(%2d) | ", 
                state.memAddrA, state.memAddrB, state.aluOut, state.ioIn, state.ioOut);
        fprintf(stdout, "Mem  : 0(%2d), 1(%2d), 2(%2d), 3(%2d), 4(%2d), 5(%2d), 6(%2d)\n",
                gSimRegisters[0], gSimRegisters[1], gSimRegisters[2], gSimRegisters[3], 
                gSimRegisters[4], gSimRegisters[5], gSimRegisters[6]);
    }
}
