library IEEE;
use IEEE.std_logic_1164.all;


entity IO is
    generic (
        BITS  : integer := 8
    );
    port (
        clk  : in  std_logic;
        nrst : in  std_logic;

        IO_load : in  std_logic;                           -- Same clock domain, so it is a pulse
        IO_out  : in  std_logic_vector(BITS - 1 downto 0); -- From CPU control
        IO_rdy  : out std_logic; -- New data has been received, same clock domain (pulse)
        IO_in   : out std_logic_vector(BITS - 1 downto 0); -- To CPU control

        load    : in  std_logic;                           -- Could be async, so it is a flag
        d_in    : in  std_logic_vector(BITS - 1 downto 0); -- From the outside world
        ready   : out std_logic;                           -- Could be async, so it is a flag
        d_out   : out std_logic_vector(BITS - 1 downto 0)  -- To the outside world
    );
end entity; -- IO

architecture RTL of IO is

    signal prev_load, load_pulse : std_logic;
    signal rdy_flag : std_logic;

begin

    ready <= rdy_flag;

    clocker : process(clk)
    begin
        if (clk'event and clk = '1') then
            prev_load <= load;
            IO_rdy <= '0';

            if (nrst = '0') then
                rdy_flag <= '0';
                IO_in <= (others => '0');
                d_out <= (others => '0');
            else
                if (load /= prev_load) then
                    IO_rdy <= '1';
                    IO_in <= d_in;
                end if;

                if (IO_load = '1') then
                    rdy_flag <= not rdy_flag;
                    d_out <= IO_out;
                end if;
            end if;
        end if;
    end process;

end architecture ; -- RTL
