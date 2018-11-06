library IEEE;
use IEEE.std_logic_1164.all;


entity IO is
    generic (
        BITS  : integer := 8
    );
    port (
        clk  : in  std_logic;
        nrst : in  std_logic;

        load    : in  std_logic;
        IO_out  : in  std_logic_vector(BITS - 1 downto 0); -- From CPU control
        IO_in   : out std_logic_vector(BITS - 1 downto 0); -- To CPU control

        d_in    : in  std_logic_vector(BITS - 1 downto 0); -- From the outside world
        d_out   : out std_logic_vector(BITS - 1 downto 0)  -- To the outside world
    );
end entity; -- IO

architecture RTL of IO is

    attribute ASYNC_REG : string;

    signal d_in1,  d_in2  : std_logic_vector(BITS - 1 downto 0);

    attribute ASYNC_REG of d_in1  : signal is "TRUE";
    attribute ASYNC_REG of d_in2  : signal is "TRUE";

begin

    IO_in <= d_in2;

    clocker : process(clk)
    begin
        if (clk'event and clk = '1') then
            d_in1 <= d_in;
            d_in2 <= d_in1;

            if (nrst = '0') then
                d_out <= (others => '0');
            else
                if (load = '1') then
                    d_out <= IO_out;
                end if;
            end if;
        end if;
    end process;

end architecture ; -- RTL
