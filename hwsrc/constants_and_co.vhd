library IEEE;
use IEEE.std_logic_1164.all;

package constants_and_co is

constant ALU_NOP   : std_logic_vector(1 downto 0) := "00";
constant ALU_AND   : std_logic_vector(1 downto 0) := "01";
constant ALU_OR    : std_logic_vector(1 downto 0) := "10";
constant ALU_ADD   : std_logic_vector(1 downto 0) := "11";

constant SEL_ZERO  : std_logic_vector(2 downto 0) := "000";
constant SEL_MEM_A : std_logic_vector(2 downto 0) := "001";
constant SEL_MEM_B : std_logic_vector(2 downto 0) := "010";
constant SEL_IMM   : std_logic_vector(2 downto 0) := "011";
constant SEL_IO    : std_logic_vector(2 downto 0) := "100";
constant SEL_ALU   : std_logic_vector(2 downto 0) := "101";

end constants_and_co;
