library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

use work.constants_and_co.all;

entity CPU is
    generic (
        BITS : integer := 8
    );
    port (
        clk  : in  std_logic;
        nrst : in  std_logic;

        d_in  : in  std_logic_vector(BITS - 1 downto 0);
        d_out : out std_logic_vector(BITS - 1 downto 0)
    );
end entity ; -- CPU

architecture RTL of CPU is

    signal synced_nrst : std_logic;

    signal cpu_io, io_cpu : std_logic_vector(BITS - 1 downto 0);
    signal io_load : std_logic;

    signal pc  : std_logic_vector(3 downto 0);
    signal opc : std_logic_vector(63 downto 0);

    signal cpu_mem, mem_outa, mem_outb : std_logic_vector(BITS - 1 downto 0);

    signal mem_addra, mem_addrb : std_logic_vector(8 downto 0);
    signal mem_write, mem_reada, mem_readb : std_logic;

    signal alu_a, alu_b : std_logic_vector(BITS - 1 downto 0);
    signal alu_out : std_logic_vector(BITS downto 0);
    signal alu_trunc : std_logic_vector(BITS - 1 downto 0);

    signal alu_op : std_logic_vector(6 downto 0);

    signal immediate : std_logic_vector(BITS - 1 downto 0);

    signal alu_sela, alu_selb, mem_sel, io_sel : std_logic_vector(2 downto 0);
begin

    alu_trunc <= alu_out(BITS - 1 downto 0);

    with alu_sela select alu_a <=
        mem_outa        when SEL_MEM_A,
        mem_outb        when SEL_MEM_B,
        immediate       when SEL_IMM,
        io_cpu          when SEL_IO,
        alu_trunc       when SEL_ALU,
        (others => '0') when others;

    with alu_selb select alu_b <=
        mem_outa        when SEL_MEM_A,
        mem_outb        when SEL_MEM_B,
        immediate       when SEL_IMM,
        io_cpu          when SEL_IO,
        alu_trunc       when SEL_ALU,
        (others => '0') when others;

    with mem_sel select cpu_mem <=
        mem_outa        when SEL_MEM_A,
        mem_outb        when SEL_MEM_B,
        immediate       when SEL_IMM,
        io_cpu          when SEL_IO,
        alu_trunc       when SEL_ALU,
        (others => '0') when others;

    with io_sel select cpu_io <=
        mem_outa        when SEL_MEM_A,
        mem_outb        when SEL_MEM_B,
        immediate       when SEL_IMM,
        io_cpu          when SEL_IO,
        alu_trunc       when SEL_ALU,
        (others => '0') when others;

    process(clk)
    begin
        if (clk'event and clk = '1') then
            synced_nrst <= nrst;
        end if;
    end process;

    io_load <= '1' when (io_sel /= SEL_ZERO) else '0';

    io : entity work.IO
    generic map (BITS => BITS)
    port map (
        clk        => clk,
        nrst       => synced_nrst,
        load       => io_load,
        IO_out     => cpu_io,
        IO_in      => io_cpu,
        d_in       => d_in,
        d_out      => d_out);

    --opcodes : entity work.OpCode
    --generic map (
    --    INIT00     => SEL_IO & SEL_IMM & SEL_ZERO & SEL_ZERO &
    --                  "000" & '0' & "00000" & ALU_ADD & "000000000" &
    --                  X"00000001", -- calc io + 1
    --    INIT01     => SEL_ZERO & SEL_ZERO & SEL_ALU & SEL_ZERO &
    --                  "001" & '0' & "00000" & ALU_NOP & "000000001" &
    --                  X"00000000", -- store in R1
    --    INIT02     => SEL_ZERO & SEL_ZERO & SEL_ZERO & SEL_ZERO &
    --                  "100" & '0' & "00000" & ALU_NOP & "000000001" &
    --                  X"00000000", -- load R1
    --    INIT03     => SEL_MEM_A & SEL_IMM & SEL_ZERO & SEL_ZERO &
    --                  "000" & '0' & "00000" & ALU_ADD & "000000000" &
    --                  X"00001011", -- R1 + 0b1011
    --    INIT04     => SEL_ALU & SEL_IMM & SEL_ALU & SEL_ZERO &
    --                  "001" & '0' & "00000" & ALU_AND & "000000010" &
    --                  X"00000111", -- store in R2, and ans & 0b111
    --    INIT05     => SEL_ZERO & SEL_ZERO & SEL_ALU & SEL_ALU &
    --                  "001" & '1' & "00000" & ALU_NOP & "000000011" &
    --                  "000000011" & "000" & X"00000", -- store in R3 and IO out
    --    INIT06     => SEL_ZERO & SEL_ZERO & SEL_ZERO & SEL_ZERO &
    --                  "010" & '1' & "00000" & ALU_NOP & "000000000" &
    --                  "000000011" & "000" & X"00000", -- load R3
    --    INIT07     => SEL_ZERO & SEL_ZERO & SEL_ZERO & SEL_MEM_B &
    --                  "010" & '1' & "00000" & ALU_NOP & "000000000" &
    --                  "000000010" & "000" & X"00000", -- load R2, store R3 in IO
    --    INIT08     => SEL_ZERO & SEL_ZERO & SEL_ZERO & SEL_MEM_B &
    --                  "010" & '1' & "00000" & ALU_NOP & "000000000" &
    --                  "000000001" & "000" & X"00000", -- load R1, store R2 in IO
    --    INIT09     => SEL_ZERO & SEL_ZERO & SEL_ZERO & SEL_MEM_B &
    --                  "010" & '1' & "00000" & ALU_NOP & "000000000" &
    --                  "000000000" & "000" & X"00000", -- load R0, store R1 in IO
    --    INIT0A     => SEL_ZERO & SEL_ZERO & SEL_ZERO & SEL_MEM_B &
    --                  "000" & '0' & "00000" & ALU_NOP & "000000000" &
    --                  "000000000" & "000" & X"00000"  -- store R0 in IO
    --)
    --port map (
    --    clk        => clk,
    --    nrst       => synced_nrst,
    --    pc         => pc,
    --    opcode     => opc);
    opcodes : entity work.OpCode
    port map (
        clk      => clk,
        nrst     => synced_nrst,
        pc       => pc,
        opcode   => opc);


    registers : entity work.Registers
    generic map (
        BITS       => BITS,
        DEPTH      => 32,
        DEPTH_LOG  => 5
    )
    port map (
        clk        => clk,
        nrst       => synced_nrst,
        data_in    => cpu_mem,
        we         => mem_write,
        addr_a     => mem_addra(4 downto 0),
        rd_a       => mem_reada,
        addr_b     => mem_addrb(4 downto 0),
        rd_b       => mem_readb,
        data_out_a => mem_outa,
        data_out_b => mem_outb);

    alu : entity work.ALU
    generic map (BITS => BITS)
    port map (
        clk        => clk,
        nrst       => synced_nrst,
        a          => alu_a,
        b          => alu_b,
        op         => alu_op(1 downto 0),
        p          => alu_out);

    control : entity work.Controller
    generic map (BITS => BITS)
    port map (
        clk        => clk,
        nrst       => synced_nrst,
        opc        => opc,
        pc         => pc,
        immediate  => immediate,
        mem_write  => mem_write,
        mem_reada  => mem_reada,
        mem_readb  => mem_readb,
        mem_addra  => mem_addra,
        mem_addrb  => mem_addrb,
        alu_op     => alu_op,
        alu_sela   => alu_sela,
        alu_selb   => alu_selb,
        mem_sel    => mem_sel,
        io_sel     => io_sel);

    --control : entity work.Controller
    --generic map (BITS => BITS)
    --port map (
    --    clk        => clk,
    --    nrst       => synced_nrst,
    --    opc        => opc,
    --    pc         => pc,
    --    immediate  => immediate,
    --    mem_write  => mem_write,
    --    mem_reada  => mem_reada,
    --    mem_readb  => mem_readb,
    --    mem_addra  => mem_addra,
    --    mem_addrb  => mem_addrb,
    --    alu_op     => alu_op,
    --    alu_sela   => alu_sela,
    --    alu_selb   => alu_selb,
    --    mem_sel    => mem_sel,
    --    io_sel     => io_sel);

end architecture ; -- RTL