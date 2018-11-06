library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity Controller is
    generic (
        BITS : integer := 8
    );
    port (
        clk       : in  std_logic;
        nrst      : in  std_logic;

        opc       : in  std_logic_vector(63 downto 0);

        pc        : out std_logic_vector(5 downto 0);
        immediate : out std_logic_vector(BITS - 1 downto 0);

        mem_write : out std_logic;
        mem_reada : out std_logic;
        mem_readb : out std_logic;
        mem_addra : out std_logic_vector(4 downto 0);
        mem_addrb : out std_logic_vector(4 downto 0);

        alu_op    : out std_logic_vector(1 downto 0);

        alu_sela  : out std_logic_vector(2 downto 0);
        alu_selb  : out std_logic_vector(2 downto 0);
        mem_sel   : out std_logic_vector(2 downto 0);
        io_sel    : out std_logic_vector(2 downto 0)
    );
end entity ; -- Controller

architecture FSM of Controller is

    constant ALU_A_START    : integer := 63;
    constant ALU_B_START    : integer := 60;
    constant MEM_START      : integer := 57;
    constant IO_START       : integer := 54;
    constant MEM_A_RD       : integer := 51;
    constant MEM_B_RD       : integer := 50;
    constant MEM_WE         : integer := 49;
    constant MEM_AS_B       : integer := 48;
    constant ALU_OP_START   : integer := 47;
    constant MEM_A_START    : integer := 40;
    constant MEM_B_START    : integer := 31;
    constant MEM_B_END      : integer := 23;
    constant IMM_START      : integer := 31;

    signal pc_counter       : unsigned(5 downto 0);

    signal alu_op_int       : std_logic_vector(6 downto 0);
    signal mem_addra_int    : std_logic_vector(8 downto 0);
    signal mem_addrb_int    : std_logic_vector(8 downto 0);
    signal immediate_int    : std_logic_vector(31 downto 0);

begin

    pc        <= std_logic_vector(pc_counter);
    immediate <= immediate_int(BITS - 1 downto 0);

    mem_write <= opc(MEM_WE);
    mem_reada <= opc(MEM_A_RD);
    mem_readb <= opc(MEM_B_RD);
    mem_addra <= mem_addra_int(4 downto 0);
    mem_addrb <= mem_addrb_int(4 downto 0);

    alu_op    <= alu_op_int(1 downto 0);

    alu_sela  <= opc(ALU_A_START downto ALU_B_START + 1);
    alu_selb  <= opc(ALU_B_START downto MEM_START + 1);
    mem_sel   <= opc(MEM_START   downto IO_START + 1);
    io_sel    <= opc(IO_START    downto MEM_A_RD + 1);

    fsm_clk : process(clk)
    begin
        if (clk'event and clk = '1') then
            if (nrst = '0') then
                pc_counter <= (others => '0');
            else
                pc_counter <= pc_counter + 1;
            end if;
        end if;
    end process;

    alu_op_int    <= opc(ALU_OP_START downto MEM_A_START + 1);
    mem_addra_int <= opc(MEM_A_START downto MEM_B_START + 1);
    mem_addrb_int <= opc(MEM_B_START downto MEM_B_END) when (opc(MEM_AS_B) = '1') else (others => '0');
    immediate_int <= opc(IMM_START downto 0) when (opc(MEM_AS_B) = '0') else (others => '0');

end architecture; -- FSM

--architecture FSM of Controller is

--    type ControlState is (
--        Control_Idle,
--        Control_Read,
--        Control_Modify,
--        Control_Write
--    );

--    signal state, next_state : ControlState;
--    signal pc_counter, npc_counter : unsigned(5 downto 0);

--begin

--    pc <= std_logic_vector(pc_counter);

--    fsm_clk : process(clk)
--    begin
--        if (clk'event and clk = '1') then
--            if (nrst = '0') then
--                state <= Control_Idle;
--                pc_counter <= (others => '0');
--            else
--                state <= next_state;
--                pc_counter <= npc_counter;
--            end if;
--        end if;
--    end process;

--    fsm_input : process(state, pc_counter)
--    begin
--        next_state  <= state;
--        npc_counter <= pc_counter;

--        case (state) is
--            when Control_Idle =>
--                next_state  <= Control_Read;
--            when Control_Read =>
--                next_state <= Control_Modify;
--            when Control_Modify =>
--                next_state <= Control_Write;
--            when Control_Write =>
--                next_state <= Control_Idle;
--                npc_counter <= pc_counter + 1;
--            when others => null;
--        end case;
--    end process;

--    fsm_output : process(state, opc)
--    begin
--        immediate  <= (others => '0');
--        mem_write  <= '0';
--        mem_reada  <= '0';
--        mem_readb  <= '0';
--        mem_addra  <= (others => '0');
--        mem_addrb  <= (others => '0');
--        alu_op     <= (others => '0');
--        alu_sela   <= (others => '0');
--        alu_selb   <= (others => '0');
--        mem_sel    <= (others => '0');
--        io_sel     <= (others => '0');

--        case (state) is
--            when Control_Read =>
--                mem_reada <= '1';
--                mem_readb <= '1';

--            when others => null;
--        end case;
--    end process;

--end architecture ; -- FSM
