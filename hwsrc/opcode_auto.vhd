-- Generated with the TURD machine 0.v.X9.y --

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity OpCode is
    generic (
        BITS   : integer := 64;
        DEPTH  : integer := 4
    );
    port (
        clk    : in  std_logic;
        nrst   : in  std_logic;

        pc     : in  std_logic_vector(DEPTH - 1 downto 0);
        opcode : out std_logic_vector(BITS - 1 downto 0)
    );
end entity; -- OpCode

architecture RTL of OpCode is

    type rom_block is array(0 to 15) of std_logic_vector(BITS - 1 downto 0);

    signal rom_mem : rom_block := (
         0 => X"A000000000000000",
         1 => X"8C00060000000001",
         2 => X"AE8206000000000B",
         3 => X"AE82020100000007",
         4 => X"A282000200000000",
         5 => X"0058000100000000",
         6 => X"2C00060000000001",
         7 => X"0058000000000000",
         8 => X"0010000000000000",
         9 => X"A005000000000000",
        10 => X"680006000000000B",
        11 => X"A005000000800000",
        12 => X"A800060000000000",
        13 => X"0050000000000000",
        14 => X"0000000000000000",
        15 => X"0000000000000000"
    );

begin

    clocker : process(clk)
    begin
        if (clk'event and clk = '1') then
            if (nrst = '0') then
                opcode <= (others => '0');
            else
                opcode <= rom_mem(to_integer(unsigned(pc)));
            end if;
        end if;
    end process;

end architecture ; -- RTL

