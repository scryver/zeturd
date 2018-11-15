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
generate_bitvalue_cstr(u64 value, u32 bits)
{
    char tempBuf[bits + 1];
    value <<= (64 - bits);
    for (u32 bit = 0; bit < bits; ++bit)
    {
        if (0x8000000000000000 & value)
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

#define PRINT_CONSTANT(con, bits) fprintf(output.file, "    constant %s : std_logic_vector(%u downto 0) := \"%s\";\n", #con, bits - 1, generate_bitvalue_cstr(con, bits))

internal void
generate_constants(OpCodeStats *stats, FileStream output)
{
    generate_vhdl_header(output);
    
    fprintf(output.file, "package constants_and_co is\n\n");
    
    PRINT_CONSTANT(Alu_Noop, stats->aluOpBits);
    PRINT_CONSTANT(Alu_And, stats->aluOpBits);
    PRINT_CONSTANT(Alu_Or, stats->aluOpBits);
    PRINT_CONSTANT(Alu_Add, stats->aluOpBits);
    fprintf(output.file, "\n");
    
    PRINT_CONSTANT(Select_Zero, stats->selectBits);
    PRINT_CONSTANT(Select_MemoryA, stats->selectBits);
    PRINT_CONSTANT(Select_MemoryB, stats->selectBits);
    PRINT_CONSTANT(Select_Immediate, stats->selectBits);
    PRINT_CONSTANT(Select_IO, stats->selectBits);
    PRINT_CONSTANT(Select_Alu, stats->selectBits);
    fprintf(output.file, "\n");
    
    fprintf(output.file, "end constants_and_co;\n");
}

#undef PRINT_CONSTANT

internal void
generate_opcode_vhdl(OpCodeStats *stats, OpCode *opCodes, FileStream output)
{
    generate_vhdl_header(output);
    
    fprintf(output.file, "entity OpCode is\n");
    fprintf(output.file, "    port (\n");
    fprintf(output.file, "        clk    : in  std_logic;\n");
    fprintf(output.file, "        nrst   : in  std_logic;\n\n");
    fprintf(output.file, "        pc     : in  std_logic_vector(%u downto 0);\n", stats->opCodeBits - 1);
    fprintf(output.file, "        opc    : out std_logic_vector(%u downto 0)\n", stats->opCodeBitWidth - 1);
    fprintf(output.file, "    );\n");
    fprintf(output.file, "end entity; -- OpCode\n\n");
    
    fprintf(output.file, "architecture RTL of OpCode is\n\n");
    fprintf(output.file, "    type rom_block is array(0 to %u) ", (1 << stats->opCodeBits) - 1);
    fprintf(output.file, "of std_logic_vector(%u downto 0);\n\n", stats->opCodeBitWidth - 1);
    fprintf(output.file, "    signal rom_mem : rom_block := (\n");
    for (u32 opcIdx = 0; opcIdx < stats->opCodeCount; ++opcIdx)
    {
        u64 opcValue = opcode_packing(stats, &opCodes[opcIdx]);
        fprintf(output.file, "         %2u => \"%s\"%s\n", opcIdx, generate_bitvalue_cstr(opcValue, stats->opCodeBitWidth),
                opcIdx < ((1 << stats->opCodeBits) - 1) ? "," : "");
    }
    for (u32 opcIdx = stats->opCodeCount; opcIdx < (1 << stats->opCodeBits); ++opcIdx)
    {
        fprintf(output.file, "        %2u => \"%s\"%s\n", opcIdx, generate_bitvalue_cstr(0UL, stats->opCodeBitWidth),
                opcIdx < ((1 << stats->opCodeBits) - 1) ? "," : "");
    }
    fprintf(output.file, "    );\n\n");
    
    fprintf(output.file, "begin\n\n");
    fprintf(output.file, "    clocker : process(clk)\n");
    fprintf(output.file, "    begin\n");
    fprintf(output.file, "        if (clk'event and clk = '1') then\n");
    fprintf(output.file, "            if (nrst = '0') then\n");
    fprintf(output.file, "                opc <= (others => '0');\n");
    fprintf(output.file, "            else\n");
    fprintf(output.file, "                opc <= rom_mem(to_integer(unsigned(pc)));\n");
    fprintf(output.file, "            end if;\n");
    fprintf(output.file, "        end if;\n");
    fprintf(output.file, "    end process;\n\n");
    
    fprintf(output.file, "end architecture ; -- RTL\n\n");
}

internal void
generate_controller(OpCodeStats *stats, FileStream output)
{
    i_expect(stats->bitWidth > 0);
    i_expect(stats->bitWidth <= 32);
    
    generate_vhdl_header(output);
    fprintf(output.file, "entity Controller is\n");
    fprintf(output.file, "    generic (\n");
    fprintf(output.file, "        BITS : integer := %u\n", stats->bitWidth);
    fprintf(output.file, "    );\n");
    fprintf(output.file, "    port (\n");
    fprintf(output.file, "        clk       : in  std_logic;\n");
    fprintf(output.file, "        nrst      : in  std_logic;\n\n");
    fprintf(output.file, "        io_rdy    : in  std_logic;\n");
    fprintf(output.file, "        opc       : in  std_logic_vector(%u downto 0);\n\n", stats->opCodeBitWidth - 1);
    fprintf(output.file, "        pc        : out std_logic_vector(%u downto 0);\n", stats->opCodeBits - 1);
    fprintf(output.file, "        immediate : out std_logic_vector(BITS - 1 downto 0);\n\n");
    fprintf(output.file, "        mem_write : out std_logic;\n");
    fprintf(output.file, "        mem_reada : out std_logic;\n");
    fprintf(output.file, "        mem_readb : out std_logic;\n");
    fprintf(output.file, "        mem_addra : out std_logic_vector(%u downto 0);\n", stats->addressBits - 1);
    fprintf(output.file, "        mem_addrb : out std_logic_vector(%u downto 0);\n\n", stats->addressBits - 1);
    fprintf(output.file, "        alu_op    : out std_logic_vector(%u downto 0);\n\n", stats->aluOpBits - 1);
    fprintf(output.file, "        alu_sela  : out std_logic_vector(%u downto 0);\n", stats->selectBits - 1);
    fprintf(output.file, "        alu_selb  : out std_logic_vector(%u downto 0);\n", stats->selectBits - 1);
    fprintf(output.file, "        mem_sel   : out std_logic_vector(%u downto 0);\n", stats->selectBits - 1);
    fprintf(output.file, "        io_sel    : out std_logic_vector(%u downto 0)\n", stats->selectBits - 1);
    fprintf(output.file, "    );\n");
    fprintf(output.file, "end entity ; -- Controller\n\n");
    
    fprintf(output.file, "architecture FSM of Controller is\n\n");
    fprintf(output.file, "    signal pc_counter       : unsigned(%u downto 0);\n\n", stats->opCodeBits - 1);
    fprintf(output.file, "    signal immediate_inter  : std_logic_vector(%u downto 0);\n\n", ((stats->immediateBits < stats->bitWidth) ? stats->bitWidth : stats->immediateBits) - 1);
    fprintf(output.file, "begin\n\n");
    
    fprintf(output.file, "    pc        <= std_logic_vector(pc_counter);\n");
    if (stats->immediateBits < stats->bitWidth)
    {
        fprintf(output.file, "    immediate(%u downto 0) <= opc(%u downto %u) when (opc(%u) = '0') else (others => '0');\n", stats->immediateBits - 1,
                get_offset_immediate(stats) + stats->immediateBits - 1, get_offset_immediate(stats), get_offset_useB(stats));
        fprintf(output.file, "    immediate(%u downto %u) <= (others => '0');\n\n",
                stats->bitWidth - 1, stats->immediateBits);
    }
    else
    {
                fprintf(output.file, "    immediate <= opc(BITS - 1 downto 0) when (opc(%u) = '0') else (others => '0');\n\n", get_offset_useB(stats));
    }
    
    fprintf(output.file, "    mem_write <= opc(%u);\n", get_offset_mem_write(stats));
    fprintf(output.file, "    mem_reada <= opc(%u);\n", get_offset_mem_read_a(stats));
    fprintf(output.file, "    mem_readb <= opc(%u);\n", get_offset_mem_read_b(stats));
    fprintf(output.file, "    mem_addra <= opc(%u downto %u);\n",
            get_offset_addr_a(stats) + stats->addressBits - 1, get_offset_addr_a(stats));
    fprintf(output.file, "    mem_addrb <= opc(%u downto %u) when (opc(%u) = '1') "
            "else (others => '0');\n\n", 
            get_offset_addr_b(stats) + stats->addressBits - 1, get_offset_addr_b(stats), get_offset_useB(stats));
    fprintf(output.file, "    alu_op    <= opc(%u downto %u);\n\n",
            get_offset_alu_op(stats) + stats->aluOpBits - 1, get_offset_alu_op(stats));
    fprintf(output.file, "    alu_sela  <= opc(%u downto %u);\n",
            get_offset_sel_alu_a(stats) + stats->selectBits - 1, get_offset_sel_alu_a(stats));
    fprintf(output.file, "    alu_selb  <= opc(%u downto %u);\n",
            get_offset_sel_alu_b(stats) + stats->selectBits - 1, get_offset_sel_alu_b(stats));
    fprintf(output.file, "    mem_sel   <= opc(%u downto %u);\n",
            get_offset_sel_mem(stats) + stats->selectBits - 1, get_offset_sel_mem(stats));
    fprintf(output.file, "    io_sel    <= opc(%u downto %u);\n\n",
            get_offset_sel_io(stats) + stats->selectBits - 1, get_offset_sel_io(stats));
    
    fprintf(output.file, "    fsm_clk : process(clk)\n");
    fprintf(output.file, "    begin\n");
    fprintf(output.file, "        if (clk'event and clk = '1') then\n");
    fprintf(output.file, "            if (nrst = '0') then\n");
    fprintf(output.file, "                pc_counter <= (others => '0');\n");
    fprintf(output.file, "            else\n");
    String pcCountMax = generate_bitvalue(stats->opCodeCount - 1, stats->opCodeBits);
    if (stats->synced)
    {
        String pcZero = generate_bitvalue(0, stats->opCodeBits);
        fprintf(output.file,
                "                if (pc_counter = \"%.*s\") and (io_rdy = '1') then\n",
                pcZero.size, pcZero.data);
        fprintf(output.file, "                    pc_counter <= pc_counter + 1;\n");
        fprintf(output.file,
                "                elsif (pc_counter /= \"%.*s\") and (pc_counter < \"%.*s\") then",
                pcZero.size, pcZero.data, pcCountMax.size, pcCountMax.data);
    }
    else
    {
    fprintf(output.file, "                if (pc_counter < \"%.*s\") then\n",
                pcCountMax.size, pcCountMax.data);
    }
    fprintf(output.file, "                    pc_counter <= pc_counter + 1;\n");
    fprintf(output.file, "                else\n");
    fprintf(output.file, "                    pc_counter <= (others => '0');\n");
    fprintf(output.file, "                end if;\n");
    fprintf(output.file, "            end if;\n");
    fprintf(output.file, "        end if;\n");
    fprintf(output.file, "    end process;\n\n");
    
    fprintf(output.file, "end architecture; -- FSM\n\n");
}

internal void
generate_registers(OpCodeStats *stats, FileStream output)
{
    generate_vhdl_header(output);
    
    fprintf(output.file, "entity Registers is\n");
    fprintf(output.file, "    generic (\n");
    fprintf(output.file, "        BITS   : integer := %u\n", stats->bitWidth);
    fprintf(output.file, "    );\n");
    fprintf(output.file, "    port (\n");
    fprintf(output.file, "        clk     : in  std_logic;\n");
    fprintf(output.file, "        nrst    : in  std_logic;\n\n");
    fprintf(output.file, "        data_in : in  std_logic_vector(BITS - 1 downto 0);\n");
    fprintf(output.file, "        we      : in  std_logic;\n\n");
    fprintf(output.file, "        addr_a  : in  std_logic_vector(%u downto 0);\n", stats->addressBits - 1);
    fprintf(output.file, "        rd_a    : in  std_logic;\n");
    fprintf(output.file, "        addr_b  : in  std_logic_vector(%u downto 0);\n", stats->addressBits - 1);
    fprintf(output.file, "        rd_b    : in  std_logic;\n\n");
    fprintf(output.file, "        data_out_a : out std_logic_vector(BITS - 1 downto 0);\n");
    fprintf(output.file, "        data_out_b : out std_logic_vector(BITS - 1 downto 0)\n");
    fprintf(output.file, "    );\n");
    fprintf(output.file, "end entity; -- Registers\n\n");
    
    fprintf(output.file, "architecture RTL of Registers is\n\n");
    fprintf(output.file, "    type ram_block is array(0 to %u) ", (1 << stats->addressBits) - 1);
    fprintf(output.file, "of std_logic_vector(BITS - 1 downto 0);\n\n");
    fprintf(output.file, "    signal ram_mem : ram_block := (others => (others => '0'));\n\n");
    fprintf(output.file, "    signal out_a, out_b : std_logic_vector(BITS - 1 downto 0);\n\n");
    
    fprintf(output.file, "begin\n\n");
    fprintf(output.file, "    data_out_a <= out_a;\n");
    fprintf(output.file, "    data_out_b <= out_b;\n\n");
    fprintf(output.file, "    clocker : process(clk)\n");
    fprintf(output.file, "    begin\n");
    fprintf(output.file, "        if (clk'event and clk = '1') then\n");
    fprintf(output.file, "            if (nrst = '0') then\n");
    fprintf(output.file, "                out_a <= (others => '0');\n");
    fprintf(output.file, "                out_b <= (others => '0');\n");
    fprintf(output.file, "            else\n");
    fprintf(output.file, "                -- Read\n");
    fprintf(output.file, "                if (rd_a = '1') then\n");
    //fprintf(output.file, "                    if (we = '1') then\n");
    //fprintf(output.file, "                        out_a <= data_in;\n");
    //fprintf(output.file, "                    else\n");
    fprintf(output.file, "                        out_a <= ram_mem(to_integer(unsigned(addr_a)));\n");
    //fprintf(output.file, "                    end if;\n");
    fprintf(output.file, "                else\n");
    fprintf(output.file, "                    out_a <= (others => '0');\n");
    fprintf(output.file, "                end if;\n");
    fprintf(output.file, "                if (rd_b = '1') then\n");
    fprintf(output.file, "                    out_b <= ram_mem(to_integer(unsigned(addr_b)));\n");
    fprintf(output.file, "                else\n");
    fprintf(output.file, "                    out_b <= (others => '0');\n");
    fprintf(output.file, "                end if;\n\n");
    fprintf(output.file, "                -- Write\n");
    fprintf(output.file, "                if (we = '1') then\n");
    fprintf(output.file, "                    ram_mem(to_integer(unsigned(addr_a))) <= data_in;\n");
    fprintf(output.file, "                end if;\n");
    fprintf(output.file, "            end if;\n");
    fprintf(output.file, "        end if;\n");
    fprintf(output.file, "    end process;\n\n");
    fprintf(output.file, "end architecture ; -- RTL\n\n");
}

#define PRINT_OPTION(name, type) fprintf(output.file, "        %s when %s,\n", name, #type)
#define PRINT_CASE_OPTION(name, assign, type) fprintf(output.file, "                when %s => %s <= %s;\n", #type, assign, name);
internal void
generate_select_statement_(char *name, char *selName, FileStream output)
{
    #if 0
    fprintf(output.file, "    proc_%s : process(clk)\n", name);
    fprintf(output.file, "    begin\n");
    fprintf(output.file, "        if (clk'event and clk = '1') then\n");
    fprintf(output.file, "            case (%s) is\n", selName);
    PRINT_CASE_OPTION("mem_outa ", name, Select_MemoryA);
    PRINT_CASE_OPTION("mem_outb ", name, Select_MemoryB);
    PRINT_CASE_OPTION("immediate", name, Select_Immediate);
    PRINT_CASE_OPTION("io_cpu   ", name, Select_IO);
    PRINT_CASE_OPTION("alu_trunc", name, Select_Alu);
    PRINT_CASE_OPTION("(others => '0')", name, others);
    fprintf(output.file, "            end case;\n");
    fprintf(output.file, "        end if;\n");
    fprintf(output.file, "    end process;\n\n");
    #else
    fprintf(output.file, "    with %s select %s <=\n", selName, name);
    PRINT_OPTION("mem_outa ", Select_MemoryA);
    PRINT_OPTION("mem_outb ", Select_MemoryB);
    PRINT_OPTION("immediate", Select_Immediate);
    PRINT_OPTION("io_cpu   ", Select_IO);
    PRINT_OPTION("alu_trunc", Select_Alu);
    fprintf(output.file, "        (others => '0') when others;\n\n");
    #endif
}
            #undef PRINT_CASE_OPTION
            #undef PRINT_OPTION

#define CSTR_(x) #x
#define CSTR(x) CSTR_(x)

internal void
generate_cpu_main(OpCodeStats *stats, FileStream output)
{
    i_expect(stats->bitWidth <= 32);
    i_expect(stats->bitWidth > 0);
    
    generate_vhdl_header(output);
    
    fprintf(output.file, "use work.constants_and_co.all;\n\n");
    
    fprintf(output.file, "entity CPU is\n");
    fprintf(output.file, "    generic (\n");
    fprintf(output.file, "        BITS   : integer := %u\n", stats->bitWidth);
    fprintf(output.file, "    );\n");
    fprintf(output.file, "    port (\n");
    fprintf(output.file, "        clk     : in  std_logic;\n");
    fprintf(output.file, "        nrst    : in  std_logic;\n\n");
    fprintf(output.file, "        load    : in  std_logic;\n");
    fprintf(output.file, "        d_in    : in  std_logic_vector(BITS - 1 downto 0);\n");
    fprintf(output.file, "        ready   : out std_logic;\n");
    fprintf(output.file, "        d_out   : out std_logic_vector(BITS - 1 downto 0)\n");
    fprintf(output.file, "    );\n");
    fprintf(output.file, "end entity; -- CPU\n\n");
    
    fprintf(output.file, "architecture RTL of CPU is\n\n");
    fprintf(output.file, "    signal synced_nrst : std_logic;\n\n");
    fprintf(output.file, "    signal cpu_io, io_cpu : std_logic_vector(BITS - 1 downto 0);\n");
    fprintf(output.file, "    signal io_load, io_rdy : std_logic;\n\n");
    fprintf(output.file, "    signal pc  : std_logic_vector(%u downto 0);\n", stats->opCodeBits - 1);
    fprintf(output.file, "    signal opc : std_logic_vector(%u downto 0);\n\n", stats->opCodeBitWidth - 1);
    fprintf(output.file, "    signal cpu_mem, mem_outa, mem_outb : std_logic_vector(BITS - 1 downto 0);\n\n");
    fprintf(output.file, "    signal mem_addra, mem_addrb : std_logic_vector(%u downto 0);\n", stats->addressBits - 1);
    fprintf(output.file, "    signal mem_write, mem_reada, mem_readb : std_logic;\n\n");
    fprintf(output.file, "    signal alu_a, alu_b : std_logic_vector(BITS - 1 downto 0);\n");
    fprintf(output.file, "    signal alu_out : std_logic_vector(BITS downto 0);\n");
    fprintf(output.file, "    signal alu_trunc : std_logic_vector(BITS - 1 downto 0);\n\n");
    fprintf(output.file, "    signal alu_op : std_logic_vector(%u downto 0);\n\n", stats->aluOpBits - 1);
    fprintf(output.file, "    signal immediate : std_logic_vector(BITS - 1 downto 0);\n\n");
    fprintf(output.file, "    signal alu_sela, alu_selb, mem_sel, io_sel : std_logic_vector(%u downto 0);\n\n", stats->selectBits - 1);
    
    fprintf(output.file, "begin\n\n");
    fprintf(output.file, "    alu_trunc <= alu_out(BITS - 1 downto 0);\n\n");
    generate_select_statement_("alu_a", "alu_sela", output);
    generate_select_statement_("alu_b", "alu_selb", output);
    generate_select_statement_("cpu_mem", "mem_sel", output);
    generate_select_statement_("cpu_io", "io_sel", output);
    fprintf(output.file, "    process(clk)\n");
    fprintf(output.file, "    begin\n");
    fprintf(output.file, "        if (clk'event and clk = '1') then\n");
    fprintf(output.file, "            synced_nrst <= nrst;\n");
    fprintf(output.file, "        end if;\n");
    fprintf(output.file, "    end process;\n\n");
    
    fprintf(output.file, "    io_load <= '1' when (io_sel /= %s) else '0';\n\n", CSTR(Select_Zero));
    fprintf(output.file, "    io : entity work.IO\n");
    fprintf(output.file, "    generic map (BITS => BITS)\n");
    fprintf(output.file, "    port map (\n");
    fprintf(output.file, "        clk        => clk,\n");
    fprintf(output.file, "        nrst       => synced_nrst,\n");
    fprintf(output.file, "        IO_load    => io_load,\n");
    fprintf(output.file, "        IO_out     => cpu_io,\n");
    fprintf(output.file, "        IO_rdy     => io_rdy,\n");
    fprintf(output.file, "        IO_in      => io_cpu,\n");
    fprintf(output.file, "        load       => load,\n");
    fprintf(output.file, "        d_in       => d_in,\n");
    fprintf(output.file, "        ready      => ready,\n");
    fprintf(output.file, "        d_out      => d_out);\n\n");
    
    fprintf(output.file, "    opcodes : entity work.OpCode\n");
    fprintf(output.file, "    port map (\n");
    fprintf(output.file, "        clk      => clk,\n");
    fprintf(output.file, "        nrst     => synced_nrst,\n");
    fprintf(output.file, "        pc       => pc,\n");
    fprintf(output.file, "        opc      => opc);\n\n");
    
    fprintf(output.file, "    registers : entity work.Registers\n");
    fprintf(output.file, "    generic map (\n");
    fprintf(output.file, "        BITS       => BITS\n");
    fprintf(output.file, "    )\n");
    fprintf(output.file, "    port map (\n");
    fprintf(output.file, "        clk        => clk,\n");
    fprintf(output.file, "        nrst       => synced_nrst,\n");
    fprintf(output.file, "        data_in    => cpu_mem,\n");
    fprintf(output.file, "        we         => mem_write,\n");
    fprintf(output.file, "        addr_a     => mem_addra,\n");
    fprintf(output.file, "        rd_a       => mem_reada,\n");
    fprintf(output.file, "        addr_b     => mem_addrb,\n");
    fprintf(output.file, "        rd_b       => mem_readb,\n");
    fprintf(output.file, "        data_out_a => mem_outa,\n");
    fprintf(output.file, "        data_out_b => mem_outb);\n\n");
    
    fprintf(output.file, "    alu : entity work.ALU\n");
    fprintf(output.file, "    generic map (BITS => BITS)\n");
    fprintf(output.file, "    port map (\n");
    fprintf(output.file, "        clk        => clk,\n");
    fprintf(output.file, "        nrst       => synced_nrst,\n");
    fprintf(output.file, "        a          => alu_a,\n");
    fprintf(output.file, "        b          => alu_b,\n");
    fprintf(output.file, "        op         => alu_op,\n");
    fprintf(output.file, "        p          => alu_out);\n\n");
    
    fprintf(output.file, "    control : entity work.Controller\n");
    fprintf(output.file, "    generic map (BITS => BITS)\n");
    fprintf(output.file, "    port map (\n");
    fprintf(output.file, "        clk        => clk,\n");
    fprintf(output.file, "        nrst       => synced_nrst,\n");
    fprintf(output.file, "        io_rdy     => io_rdy,\n");
    fprintf(output.file, "        opc        => opc,\n");
    fprintf(output.file, "        pc         => pc,\n");
    fprintf(output.file, "        immediate  => immediate,\n");
    fprintf(output.file, "        mem_write  => mem_write,\n");
    fprintf(output.file, "        mem_reada  => mem_reada,\n");
    fprintf(output.file, "        mem_readb  => mem_readb,\n");
    fprintf(output.file, "        mem_addra  => mem_addra,\n");
    fprintf(output.file, "        mem_addrb  => mem_addrb,\n");
    fprintf(output.file, "        alu_op     => alu_op,\n");
    fprintf(output.file, "        alu_sela   => alu_sela,\n");
    fprintf(output.file, "        alu_selb   => alu_selb,\n");
    fprintf(output.file, "        mem_sel    => mem_sel,\n");
    fprintf(output.file, "        io_sel     => io_sel);\n\n");
    
    fprintf(output.file, "end architecture ; -- RTL\n");
}

#undef CSTR_
#undef CSTR
