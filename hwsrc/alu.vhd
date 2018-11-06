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

        op   : in  std_logic_vector(1 downto 0);

        p    : out std_logic_vector(BITS downto 0)
    );
end entity; -- ALU

architecture RTL of ALU is


begin

    --op_switch : process(a, b, op)
    --begin
    --    case (op) is
    --        when ALU_NOP =>
    --            p(BITS - 1 downto 0) <= a;
    --            p(BITS) <= a(BITS - 1);
    --        when ALU_AND =>
    --            p(BITS - 1 downto 0) <= a and b;
    --            p(BITS) <= '0';
    --        when ALU_OR =>
    --            p(BITS - 1 downto 0) <= a or b;
    --            p(BITS) <= '0';
    --        when ALU_ADD =>
    --            p <= std_logic_vector(unsigned(a(BITS - 1) & a) +
    --                                  unsigned(b(BITS - 1) & b));
    --        when others =>
    --            p <= (others => '0');
    --    end case;
    --end process;

    clocker : process(clk)
    begin
        if (clk'event and clk = '1') then
            if (nrst = '0') then
                p <= (others => '0');
            else
                case (op) is
                    when ALU_NOP =>
                        p(BITS - 1 downto 0) <= a;
                        p(BITS) <= a(BITS - 1);
                    when ALU_AND =>
                        p(BITS - 1 downto 0) <= a and b;
                        p(BITS) <= '0';
                    when ALU_OR =>
                        p(BITS - 1 downto 0) <= a or b;
                        p(BITS) <= '0';
                    when ALU_ADD =>
                        p <= std_logic_vector(unsigned(a(BITS - 1) & a) +
                                              unsigned(b(BITS - 1) & b));
                    when others =>
                        null;
                end case;
            end if;
        end if;
    end process;

end architecture ; -- RTL
