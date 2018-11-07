internal String
generate_bitvalue(u32 value, u32 bits)
{
    char tempBuf[bits + 1];
    value <<= (32 - bits);
    for (u32 bit = 0; bit < bits; ++bit)
    {
        if (0x80000000 & value)
        {
            tempBuf[bit] = '1';
        }
        else
        {
            tempBuf[bit] = '0';
        }
        value <<= 1;
    }
    tempBuf[bits] = 0;
    String result = str_internalize_cstring(tempBuf);
    return result;
}

internal char *
generate_bitvalue_cstr(u32 value, u32 bits)
{
    char tempBuf[bits + 1];
    value <<= (32 - bits);
    for (u32 bit = 0; bit < bits; ++bit)
    {
        if (0x80000000 & value)
        {
            tempBuf[bit] = '1';
        }
        else
        {
            tempBuf[bit] = '0';
        }
        value <<= 1;
    }
    tempBuf[bits] = 0;
    String result = str_internalize_cstring(tempBuf);
    return (char *)result.data;
}

internal void
generate_vhdl_header(FileStream output)
{
    fprintf(output.file, "-- Generated with the TURD machine 0.v.X9.y --\n\n");
    
    fprintf(output.file, "library IEEE;\n");
    fprintf(output.file, "use IEEE.std_logic_1164.all;\n");
    fprintf(output.file, "use IEEE.numeric_std.all;\n\n");
}

internal void
generate_constants(FileStream output)
{
    generate_vhdl_header(output);
    
    fprintf(output.file, "package constants_and_co is\n\n");
    
    fprintf(output.file, "    constant ALU_NOP   : std_logic_vector(1 downto 0) := \"%s\";\n", generate_bitvalue_cstr(Alu_Noop, 2));
    fprintf(output.file, "    constant ALU_AND   : std_logic_vector(1 downto 0) := \"%s\";\n", generate_bitvalue_cstr(Alu_And, 2));
    fprintf(output.file, "    constant ALU_OR    : std_logic_vector(1 downto 0) := \"%s\";\n", generate_bitvalue_cstr(Alu_Or, 2));
    fprintf(output.file, "    constant ALU_ADD   : std_logic_vector(1 downto 0) := \"%s\";\n\n", generate_bitvalue_cstr(Alu_Add, 2));
    
    fprintf(output.file, "    constant SEL_ZERO  : std_logic_vector(2 downto 0) := \"%s\";\n", generate_bitvalue_cstr(Select_Zero, 3));
    fprintf(output.file, "    constant SEL_MEM_A : std_logic_vector(2 downto 0) := \"%s\";\n", generate_bitvalue_cstr(Select_MemoryA, 3));
    fprintf(output.file, "    constant SEL_MEM_B : std_logic_vector(2 downto 0) := \"%s\";\n", generate_bitvalue_cstr(Select_MemoryB, 3));
    fprintf(output.file, "    constant SEL_IMM   : std_logic_vector(2 downto 0) := \"%s\";\n", generate_bitvalue_cstr(Select_Immediate, 3));
    fprintf(output.file, "    constant SEL_IO    : std_logic_vector(2 downto 0) := \"%s\";\n", generate_bitvalue_cstr(Select_IO, 3));
    fprintf(output.file, "    constant SEL_ALU   : std_logic_vector(2 downto 0) := \"%s\";\n\n", generate_bitvalue_cstr(Select_Alu, 3));
    
    fprintf(output.file, "end constants_and_co;\n");
}

internal void
generate_opcode_usage(FileStream output)
{
    fprintf(output.file, "    opcodes : entity work.OpCode\n");
    fprintf(output.file, "    port map (\n");
    fprintf(output.file, "        clk      => clk,\n");
    fprintf(output.file, "        nrst     => synced_nrst,\n");
    fprintf(output.file, "        pc       => pc,\n");
    fprintf(output.file, "        opcode   => opc);\n\n");
}

internal void
generate_opcode_vhdl(OpCode *opCodes, FileStream output)
{
    u32 opCount = buf_len(opCodes);
    // NOTE(michiel): Calc next power of 2
    u32 bitPos = 0;
    while (opCount >>= 1)
    {
        ++bitPos;
    }
    if ((1 << bitPos) < buf_len(opCodes))
    {
        ++bitPos;
    }
    opCount = 1 << bitPos;
    
    generate_vhdl_header(output);
    
    fprintf(output.file, "entity OpCode is\n");
    fprintf(output.file, "    generic (\n");
    fprintf(output.file, "        BITS   : integer := 64;\n");
    fprintf(output.file, "        DEPTH  : integer := %u\n", bitPos);
    fprintf(output.file, "    );\n");
    fprintf(output.file, "    port (\n");
    fprintf(output.file, "        clk    : in  std_logic;\n");
    fprintf(output.file, "        nrst   : in  std_logic;\n\n");
    fprintf(output.file, "        pc     : in  std_logic_vector(DEPTH - 1 downto 0);\n");
    fprintf(output.file, "        opcode : out std_logic_vector(BITS - 1 downto 0)\n");
    fprintf(output.file, "    );\n");
    fprintf(output.file, "end entity; -- OpCode\n\n");
    
    fprintf(output.file, "architecture RTL of OpCode is\n\n");
    fprintf(output.file, "    type rom_block is array(0 to %u) ", opCount - 1);
    fprintf(output.file, "of std_logic_vector(BITS - 1 downto 0);\n\n");
    fprintf(output.file, "    signal rom_mem : rom_block := (\n");
    for (u32 opcIdx = 0; opcIdx < buf_len(opCodes); ++opcIdx)
    {
        fprintf(output.file, "        %2u => X\"%016lX\"%s\n", opcIdx,
                opCodes[opcIdx].totalOp, opcIdx < (opCount - 1) ? "," : "");
    }
    for (u32 opcIdx = buf_len(opCodes); opcIdx < opCount; ++opcIdx)
    {
        fprintf(output.file, "        %2u => X\"%016lX\"%s\n", opcIdx, 0UL,
                opcIdx < (opCount - 1) ? "," : "");
    }
    fprintf(output.file, "    );\n\n");
    
    fprintf(output.file, "begin\n\n");
    fprintf(output.file, "    clocker : process(clk)\n");
    fprintf(output.file, "    begin\n");
    fprintf(output.file, "        if (clk'event and clk = '1') then\n");
    fprintf(output.file, "            if (nrst = '0') then\n");
    fprintf(output.file, "                opcode <= (others => '0');\n");
    fprintf(output.file, "            else\n");
    fprintf(output.file, "                opcode <= rom_mem(to_integer(unsigned(pc)));\n");
    fprintf(output.file, "            end if;\n");
    fprintf(output.file, "        end if;\n");
    fprintf(output.file, "    end process;\n\n");
    
    fprintf(output.file, "end architecture ; -- RTL\n\n");
}

internal void
generate_controller_usage(FileStream output)
{
}

internal void
generate_controller(u32 bitWidth, u32 opCodeCount, FileStream output)
{
    i_expect(bitWidth <= 32);
    i_expect(bitWidth > 0);
    
    u32 opCount = opCodeCount;
    // NOTE(michiel): Calc next power of 2
    u32 bitPos = 0;
    while (opCount >>= 1)
    {
        ++bitPos;
    }
    if ((1 << bitPos) < opCodeCount)
    {
        ++bitPos;
    }
    opCount = 1 << bitPos;
    
    generate_vhdl_header(output);
    fprintf(output.file, "entity Controller is\n");
     fprintf(output.file, "    generic (\n");
    fprintf(output.file, "        BITS : integer := %u\n", bitWidth);
    fprintf(output.file, "    );\n");
    fprintf(output.file, "    port (\n");
    fprintf(output.file, "        clk       : in  std_logic;\n");
    fprintf(output.file, "        nrst      : in  std_logic;\n\n");
    fprintf(output.file, "        opc       : in  std_logic_vector(63 downto 0);\n\n");
    fprintf(output.file, "        pc        : out std_logic_vector(%u downto 0);\n", bitPos - 1);
    fprintf(output.file, "        immediate : out std_logic_vector(BITS - 1 downto 0);\n\n");
    fprintf(output.file, "        mem_write : out std_logic;\n");
    fprintf(output.file, "        mem_reada : out std_logic;\n");
    fprintf(output.file, "        mem_readb : out std_logic;\n");
    fprintf(output.file, "        mem_addra : out std_logic_vector(%u downto 0);\n", OPC_ADDRA_BITS - 1);
    fprintf(output.file, "        mem_addrb : out std_logic_vector(%u downto 0);\n\n", OPC_ADDRB_BITS - 1);
    fprintf(output.file, "        alu_op    : out std_logic_vector(%u downto 0);\n\n", OPC_ALUOP_BITS - 1);
    fprintf(output.file, "        alu_sela  : out std_logic_vector(%u downto 0);\n", OPC_SELALA_BITS - 1);
    fprintf(output.file, "        alu_selb  : out std_logic_vector(%u downto 0);\n", OPC_SELALB_BITS - 1);
    fprintf(output.file, "        mem_sel   : out std_logic_vector(%u downto 0);\n", OPC_SELMEM_BITS - 1);
    fprintf(output.file, "        io_sel    : out std_logic_vector(%u downto 0)\n", OPC_SELIO_BITS - 1);
    fprintf(output.file, "    );\n");
    fprintf(output.file, "end entity ; -- Controller\n\n");
    
    fprintf(output.file, "architecture FSM of Controller is\n\n");
    fprintf(output.file, "    signal pc_counter       : unsigned(%u downto 0);\n\n", bitPos - 1);
    fprintf(output.file, "begin\n\n");
    
    fprintf(output.file, "    pc        <= std_logic_vector(pc_counter);\n");
    fprintf(output.file, "    immediate <= opc(%u downto 0) when (opc(%u) = '0') "
            "else (others => '0');\n\n", bitWidth - 1, OPC_USEB_OFFS); //OPC_IMM_BITS - 1, OPC_USEB_OFFS);
    fprintf(output.file, "    mem_write <= opc(%u);\n", OPC_MEMWR_OFFS);
    fprintf(output.file, "    mem_reada <= opc(%u);\n", OPC_MEMRA_OFFS);
    fprintf(output.file, "    mem_readb <= opc(%u);\n", OPC_MEMRB_OFFS);
    fprintf(output.file, "    mem_addra <= opc(%u downto %u);\n", 
            OPC_ADDRA_OFFS + OPC_ADDRA_BITS - 1, OPC_ADDRA_OFFS);
    fprintf(output.file, "    mem_addrb <= opc(%u downto %u) when (opc(%u) = '1') "
            "else (others => '0');\n\n", OPC_ADDRB_OFFS + OPC_ADDRB_BITS - 1, OPC_ADDRB_OFFS,
            OPC_USEB_OFFS);
    fprintf(output.file, "    alu_op    <= opc(%u downto %u);\n\n", 
            OPC_ALUOP_OFFS + OPC_ALUOP_BITS - 1, OPC_ALUOP_OFFS);
    fprintf(output.file, "    alu_sela  <= opc(%u downto %u);\n", 
            OPC_SELALA_OFFS + OPC_SELALA_BITS - 1, OPC_SELALA_OFFS);
    fprintf(output.file, "    alu_selb  <= opc(%u downto %u);\n", 
            OPC_SELALB_OFFS + OPC_SELALB_BITS - 1, OPC_SELALB_OFFS);
    fprintf(output.file, "    mem_sel   <= opc(%u downto %u);\n", 
            OPC_SELMEM_OFFS + OPC_SELMEM_BITS - 1, OPC_SELMEM_OFFS);
    fprintf(output.file, "    io_sel    <= opc(%u downto %u);\n\n", 
            OPC_SELIO_OFFS + OPC_SELIO_BITS - 1, OPC_SELIO_OFFS);
    
    fprintf(output.file, "    fsm_clk : process(clk)\n");
    fprintf(output.file, "    begin\n");
    fprintf(output.file, "        if (clk'event and clk = '1') then\n");
    fprintf(output.file, "            if (nrst = '0') then\n");
    fprintf(output.file, "                pc_counter <= (others => '0');\n");
    fprintf(output.file, "            else\n");
    //fprintf(output.file, "                if (pc_counter < %u) then\n", opCodeCount - 1);
    String pcCountMax = generate_bitvalue(opCodeCount - 1, bitPos);
    fprintf(output.file, "                if (pc_counter < \"%.*s\") then\n", pcCountMax.size, pcCountMax.data);
    fprintf(output.file, "                    pc_counter <= pc_counter + 1;\n");
    fprintf(output.file, "                else\n");
    fprintf(output.file, "                    pc_counter <= (others => '0');\n");
    fprintf(output.file, "                end if;\n");
    fprintf(output.file, "            end if;\n");
    fprintf(output.file, "        end if;\n");
    fprintf(output.file, "    end process;\n\n");
            
    fprintf(output.file, "end architecture; -- FSM\n\n");
}

generate_registers(u32 registerCount, FileStream output)
{
    /*
    
entity Registers is
    generic (
        BITS  : integer := 8;
        DEPTH : integer := 32;
        DEPTH_LOG : integer := 5
    );
    port (
        clk  : in  std_logic;
        nrst : in  std_logic;
        
        data_in : in  std_logic_vector(BITS - 1 downto 0);
        we      : in  std_logic;
        
        addr_a : in  std_logic_vector(DEPTH_LOG - 1 downto 0); -- Also used as write address
        rd_a   : in  std_logic;
        addr_b : in  std_logic_vector(DEPTH_LOG - 1 downto 0);
        rd_b   : in  std_logic;
        
        data_out_a : out std_logic_vector(BITS - 1 downto 0);
        data_out_b : out std_logic_vector(BITS - 1 downto 0)
    );
end entity; -- Registers

architecture RTL of Registers is

    type ram_block is array(0 to DEPTH - 1) of std_logic_vector(BITS - 1 downto 0);
    
    signal ram_mem : ram_block := (others => (others => '0'));
    
    signal out_a, out_b : std_logic_vector(BITS - 1 downto 0);
begin

    data_out_a <= out_a;
    data_out_b <= out_b;
    
    clocker : process(clk)
    begin
        if (clk'event and clk = '1') then
            if (nrst = '0') then
                out_a <= (others => '0');
                out_b <= (others => '0');
            else
                -- Read
                if (rd_a = '1') then
                    out_a <= ram_mem(to_integer(unsigned(addr_a)));
                else
                    out_a <= (others => '0');
                end if;
                if (rd_b = '1') then
                    out_b <= ram_mem(to_integer(unsigned(addr_b)));
                else
                    out_b <= (others => '0');
                end if;
                
                -- Write
                if (we = '1') then
                    ram_mem(to_integer(unsigned(addr_a))) <= data_in;
                end if;
            end if;
        end if;
    end process;
    
end architecture ; -- RTL
    */
}