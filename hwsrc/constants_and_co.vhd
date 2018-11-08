library IEEE;
use IEEE.std_logic_1164.all;

package constants_and_co is

constant Alu_Noop  : std_logic_vector(1 downto 0) := "00";
constant Alu_And   : std_logic_vector(1 downto 0) := "01";
constant Alu_Or    : std_logic_vector(1 downto 0) := "10";
constant Alu_Add   : std_logic_vector(1 downto 0) := "11";

constant Select_Zero      : std_logic_vector(2 downto 0) := "000";
constant Select_MemoryA   : std_logic_vector(2 downto 0) := "001";
constant Select_MemoryB   : std_logic_vector(2 downto 0) := "010";
constant Select_Immediate : std_logic_vector(2 downto 0) := "011";
constant Select_IO        : std_logic_vector(2 downto 0) := "100";
constant Select_Alu       : std_logic_vector(2 downto 0) := "101";

end constants_and_co;
