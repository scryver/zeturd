library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;


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
