library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity tb_cpu is
end entity tb_cpu;

architecture Testing of tb_cpu is

    constant BITS : integer := 8;
    constant TIME_DELTA : time := 20 ns;
    constant HALF_TIME  : time := 10 ns;

    signal eos       : std_logic := '0'; -- End Of Simulation

    signal clk, nrst : std_logic;
    signal din, dout : std_logic_vector(BITS - 1 downto 0);

begin

    dut : entity work.CPU
    generic map (BITS => BITS)
    port map (
        clk => clk,
        nrst => nrst,
        d_in => din,
        d_out => dout);

    clking : process -- clock process
    begin
        clk <= '0';
        wait for HALF_TIME;
        clk <= '1';
        wait for HALF_TIME;

        if (eos = '1') then
            wait;
        end if;
    end process;

    stimuli : process
    begin
        nrst <= '0';
        din <= (others => '0');

        wait until clk = '0';
        wait for 3 * TIME_DELTA;
        din <= X"12";
        wait for 5 * TIME_DELTA;
        nrst <= '1';
        wait for TIME_DELTA * 120;

        eos <= '1';
        wait;
    end process;

end architecture Testing;