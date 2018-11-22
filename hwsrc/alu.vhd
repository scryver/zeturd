library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

use work.constants_and_co.all;

entity ALU is
    generic (
        BITS  : integer := 8
    );
    port (
        clk  : in  std_logic;
        nrst : in  std_logic;

        a    : in  std_logic_vector(BITS - 1 downto 0);
        b    : in  std_logic_vector(BITS - 1 downto 0);

        op   : in  std_logic_vector(2 downto 0);

        p    : out std_logic_vector(BITS downto 0)
    );
end entity; -- ALU

architecture RTL of ALU is


begin

    clocker : process(clk)
    begin
        if (clk'event and clk = '1') then
            if (nrst = '0') then
                p <= (others => '0');
            else
                case (op) is
                    when Alu_Noop =>
                        p(BITS - 1 downto 0) <= a;
                        p(BITS) <= a(BITS - 1);
                    when Alu_And =>
                        p(BITS - 1 downto 0) <= a and b;
                        p(BITS) <= '0';
                    when Alu_Or =>
                        p(BITS - 1 downto 0) <= a or b;
                        p(BITS) <= '0';
                    when Alu_Xor =>
                        p(BITS - 1 downto 0) <= a xor b;
                        p(BITS) <= '0';
                    when Alu_Add =>
                        p <= std_logic_vector(signed(a(BITS - 1) & a) +
                                              signed(b(BITS - 1) & b));
                    when Alu_Sub =>
                        p <= std_logic_vector(signed(a(BITS - 1) & a) -
                                              signed(b(BITS - 1) & b));
                    when others =>
                        null;
                end case;
            end if;
        end if;
    end process;

end architecture ; -- RTL
