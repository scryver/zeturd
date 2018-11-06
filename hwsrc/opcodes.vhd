library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;


entity OpCode is
    generic (
        BITS   : integer := 64;
        INIT00 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT01 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT02 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT03 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT04 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT05 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT06 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT07 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT08 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT09 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT0A : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT0B : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT0C : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT0D : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT0E : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT0F : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT10 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT11 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT12 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT13 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT14 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT15 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT16 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT17 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT18 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT19 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT1A : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT1B : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT1C : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT1D : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT1E : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT1F : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT20 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT21 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT22 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT23 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT24 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT25 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT26 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT27 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT28 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT29 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT2A : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT2B : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT2C : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT2D : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT2E : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT2F : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT30 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT31 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT32 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT33 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT34 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT35 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT36 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT37 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT38 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT39 : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT3A : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT3B : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT3C : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT3D : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT3E : std_logic_vector(63 downto 0) := X"0000000000000000";
        INIT3F : std_logic_vector(63 downto 0) := X"0000000000000000"
    );
    port (
        clk    : in  std_logic;
        nrst   : in  std_logic;

        pc     : in  std_logic_vector(5 downto 0); -- Program counter
        opcode : out std_logic_vector(BITS - 1 downto 0)
    );
end entity; -- OpCode

architecture RTL of OpCode is

    type rom_block is array(0 to 63) of std_logic_vector(BITS - 1 downto 0);

    signal rom_mem : rom_block := (
         0 => INIT00,  1 => INIT01,  2 => INIT02,  3 => INIT03,
         4 => INIT04,  5 => INIT05,  6 => INIT06,  7 => INIT07,
         8 => INIT08,  9 => INIT09, 10 => INIT0A, 11 => INIT0B,
        12 => INIT0C, 13 => INIT0D, 14 => INIT0E, 15 => INIT0F,
        16 => INIT10, 17 => INIT11, 18 => INIT12, 19 => INIT13,
        20 => INIT14, 21 => INIT15, 22 => INIT16, 23 => INIT17,
        24 => INIT18, 25 => INIT19, 26 => INIT1A, 27 => INIT1B,
        28 => INIT1C, 29 => INIT1D, 30 => INIT1E, 31 => INIT1F,
        32 => INIT20, 33 => INIT21, 34 => INIT22, 35 => INIT23,
        36 => INIT24, 37 => INIT25, 38 => INIT26, 39 => INIT27,
        40 => INIT28, 41 => INIT29, 42 => INIT2A, 43 => INIT2B,
        44 => INIT2C, 45 => INIT2D, 46 => INIT2E, 47 => INIT2F,
        48 => INIT30, 49 => INIT31, 50 => INIT32, 51 => INIT33,
        52 => INIT34, 53 => INIT35, 54 => INIT36, 55 => INIT37,
        56 => INIT38, 57 => INIT39, 58 => INIT3A, 59 => INIT3B,
        60 => INIT3C, 61 => INIT3D, 62 => INIT3E, 63 => INIT3F);
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
