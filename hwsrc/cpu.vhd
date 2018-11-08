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

        load  : in  std_logic;                           -- Could be async, so it is a flag
        d_in  : in  std_logic_vector(BITS - 1 downto 0);
        ready : out std_logic;                           -- Could be async, so it is a flag
        d_out : out std_logic_vector(BITS - 1 downto 0)
    );
end entity ; -- CPU

architecture RTL of CPU is

    signal synced_nrst : std_logic;

    signal cpu_io, io_cpu : std_logic_vector(BITS - 1 downto 0);
    signal io_load, io_rdy : std_logic;

    signal pc  : std_logic_vector(5 downto 0);
    signal opc : std_logic_vector(63 downto 0);

    signal cpu_mem, mem_outa, mem_outb : std_logic_vector(BITS - 1 downto 0);

    signal mem_addra, mem_addrb : std_logic_vector(4 downto 0);
    signal mem_write, mem_reada, mem_readb : std_logic;

    signal alu_a, alu_b : std_logic_vector(BITS - 1 downto 0);
    signal alu_out : std_logic_vector(BITS downto 0);
    signal alu_trunc : std_logic_vector(BITS - 1 downto 0);

    signal alu_op : std_logic_vector(1 downto 0);

    signal immediate : std_logic_vector(BITS - 1 downto 0);

    signal alu_sela, alu_selb, mem_sel, io_sel : std_logic_vector(2 downto 0);
begin

    alu_trunc <= alu_out(BITS - 1 downto 0);

    with alu_sela select alu_a <=
        mem_outa        when Select_MemoryA,
        mem_outb        when Select_MemoryB,
        immediate       when Select_Immediate,
        io_cpu          when Select_IO,
        alu_trunc       when Select_Alu,
        (others => '0') when others;

    with alu_selb select alu_b <=
        mem_outa        when Select_MemoryA,
        mem_outb        when Select_MemoryB,
        immediate       when Select_Immediate,
        io_cpu          when Select_IO,
        alu_trunc       when Select_Alu,
        (others => '0') when others;

    with mem_sel select cpu_mem <=
        mem_outa        when Select_MemoryA,
        mem_outb        when Select_MemoryB,
        immediate       when Select_Immediate,
        io_cpu          when Select_IO,
        alu_trunc       when Select_Alu,
        (others => '0') when others;

    with io_sel select cpu_io <=
        mem_outa        when Select_MemoryA,
        mem_outb        when Select_MemoryB,
        immediate       when Select_Immediate,
        io_cpu          when Select_IO,
        alu_trunc       when Select_Alu,
        (others => '0') when others;

    process(clk)
    begin
        if (clk'event and clk = '1') then
            synced_nrst <= nrst;
        end if;
    end process;

    io_load <= '1' when (io_sel /= Select_Zero) else '0';

    io : entity work.IO
    generic map (BITS => BITS)
    port map (
        clk        => clk,
        nrst       => synced_nrst,
        IO_load    => io_load,
        IO_out     => cpu_io,
        IO_rdy     => io_rdy,
        IO_in      => io_cpu,
        load       => load,
        d_in       => d_in,
        ready      => ready,
        d_out      => d_out);

    opcodes : entity work.OpCode
    port map (
        clk      => clk,
        nrst     => synced_nrst,
        pc       => pc(3 downto 0),
        opcode   => opc);

    registers : entity work.Registers
    generic map (
        BITS       => BITS
    )
    port map (
        clk        => clk,
        nrst       => synced_nrst,
        data_in    => cpu_mem,
        we         => mem_write,
        addr_a     => mem_addra,
        rd_a       => mem_reada,
        addr_b     => mem_addrb,
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
    generic map (
        BITS => BITS,
        SYNCED => TRUE
    )
    port map (
        clk        => clk,
        nrst       => synced_nrst,
        io_rdy     => io_rdy,
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

end architecture ; -- RTL