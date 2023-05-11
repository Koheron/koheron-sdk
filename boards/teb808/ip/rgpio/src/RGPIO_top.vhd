----------------------------------------------------------------------------------
-- Company: Trenz Electronic GmbH
-- Engineer:John Hartfiel 
-- 
-- Create Date: 06.06.2017 xx:xx:xx
-- Design Name: 
-- Module Name: RGPIO - Behavioral
-- Project Name: 
-- Target Devices:  RGPIO IP
-- Tool Versions: 2017.4
-- Description: 
-- 
-- Dependencies: 
-- 
-- Revision:
-- Revision 0.2  
-- Additional Comments:
--
--
----------------------------------------------------------------------------------


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
-- library UNISIM;
-- use UNISIM.VComponents.all;

entity RGPIO_top is 
    Generic (
      C_ADD_RESERVED  : integer range 0 to 1          := 0;--0 disabled 1 enabled
      C_TYP           : integer range 0 to 2          := 0  --0 Master, 1 Slave ,2 Both 
    );
    Port ( 
		-----------------------------------------------------
		--------------------FPGAIO-BUS Ports
		--
		-- Master 
		-- -external
		RGPIO_M_CLK   : out std_logic;   --  RGPIO Clock
		RGPIO_M_RX    : in  std_logic;   -- RGPIO RXD        
		RGPIO_M_TX    : out std_logic;   -- RGPIO TXD 
    
		-- -internal data
    RGPIO_M_OUT   : out std_logic_vector(23 downto 0); --user data
    RGPIO_M_IN    : in std_logic_vector(23 downto 0); --user data
		-- -internal controll
	  RGPIO_M_RESERVED_OUT            : out  std_logic_vector(3 downto 0); -- reserved for future usage
	  RGPIO_M_RESERVED_IN             : in  std_logic_vector(3 downto 0); -- reserved for future usage
	  RGPIO_M_SLAVE_ACTIVATION_CODE   : out  std_logic_vector(3 downto 0); -- activation code from external slavefor information only
	  RGPIO_M_ENABLE                  : in  std_logic; -- Enable RGPIO
	  RGPIO_M_USRCLK                  : in  std_logic; -- RGPIO CLK
	  RGPIO_M_RESET_N                 : in  std_logic; -- RGPIO Reset
		--  
		-- Slave 
		-- -external
		RGPIO_S_CLK   : in std_logic;  --  RGPIO Clock
		RGPIO_S_RX    : in  std_logic;   -- RGPIO RXD        
		RGPIO_S_TX    : out std_logic;   -- RGPIO TXD 
    
		-- -internal data
    RGPIO_S_OUT   : out std_logic_vector(23 downto 0); --user data
    RGPIO_S_IN    : in std_logic_vector(23 downto 0); --user data
		-- -internal controll
	  RGPIO_S_RESERVED_OUT  : out  std_logic_vector(3 downto 0); -- reserved for future usage
	  RGPIO_S_RESERVED_IN   : in  std_logic_vector(3 downto 0); -- reserved for future usage
	  RGPIO_S_MASTER_ACTIVATION_CODE   : out  std_logic_vector(3 downto 0);  -- activation code from external master for information only
	  RGPIO_S_ENABLED   : out  std_logic -- Enable RGPIO
  
	);
	
end RGPIO_top;

architecture Behavioral of RGPIO_top is

component ddrspi_master is
	generic (

		C_DATA_BITS	: integer	:= 32
	);
	port (
    TX_CLK      	: out std_logic; -- Transmit Clock
    TX_DATA     	: out std_logic; -- Transmit Data
    RX_DATA     	: in std_logic; -- Receive Data

    clk	    	    : in std_logic;
    resetn	     	: in std_logic;

    ready	  	    : out std_logic;
    valid	  	    : out std_logic;

    data_out    : out std_logic_vector(C_DATA_BITS-1 downto 0);
    data_in	    : in std_logic_vector(C_DATA_BITS-1 downto 0)
	);
end component;

component ddsrpi_slave is
  Port ( 
    RX_CLK : in STD_LOGIC;
    RX_DATA : in STD_LOGIC;
    TX_DATA : out STD_LOGIC;
    user_clk : out STD_LOGIC;
    gpio_out : out STD_LOGIC_VECTOR (31 downto 0);
    gpio_in : in STD_LOGIC_VECTOR (31 downto 0);
    resetn : in STD_LOGIC;
    ready : out STD_LOGIC
  );
end component;
	

signal rgpio_m_o : std_logic_vector(31 downto 0); 		
signal rgpio_m_i : std_logic_vector(31 downto 0); 	
signal rgpio_s_o : std_logic_vector(31 downto 0); 		
signal rgpio_s_i : std_logic_vector(31 downto 0); 		

signal rgpio_m_active : std_logic; 	

signal rgpio_active_code : std_logic_vector(3 downto 0) := "1010"; 
	



begin

---------------------------------------------------------------------

  master_gen  : if (C_TYP=0 or C_TYP=2) generate

    ddrspi_master_i : ddrspi_master
    port map ( 
      clk           => RGPIO_M_USRCLK,
      resetn        => RGPIO_M_RESET_N,


      data_in        => rgpio_m_i,
      data_out       => rgpio_m_o,
         
      ready          => open,
      valid          => open,
         
      TX_CLK      	=> RGPIO_M_CLK,
      RX_DATA       => RGPIO_M_RX,
      TX_DATA       => RGPIO_M_TX
    );
    
    --activate
    -- rgpio_m_i(31 downto 28) <= rgpio_m_o(31 downto 28) when RGPIO_M_ENABLE='1' else (not rgpio_m_o(31 downto 28));
    rgpio_m_i(31 downto 28) <= rgpio_m_o(31 downto 28) when RGPIO_M_ENABLE='1' else x"F";
    RGPIO_M_SLAVE_ACTIVATION_CODE <= rgpio_m_o(31 downto 28);
    --data
    RGPIO_M_OUT(23 downto 0)  <= rgpio_m_o(23 downto 0);
    rgpio_m_i(23 downto 0)    <= RGPIO_M_IN(23 downto 0);
    --reserved
    m_res_1: if(C_ADD_RESERVED = 1) generate
      RGPIO_M_RESERVED_OUT(3 downto 0)  <= rgpio_m_o(27 downto 24);
      rgpio_m_i(27 downto 24)    <= RGPIO_M_RESERVED_IN(3 downto 0);
    end generate;
    m_res_0: if(C_ADD_RESERVED = 0) generate
      rgpio_m_i(27 downto 24)  <= rgpio_m_o(27 downto 24);
    end generate;
  end generate;
  
  slave_gen  : if (C_TYP=1 or C_TYP=2) generate
    ddsrpi_slave_i : ddsrpi_slave 
      Port map ( 
        RX_CLK  => RGPIO_S_CLK,
        RX_DATA => RGPIO_S_RX,
        TX_DATA => RGPIO_S_TX,
        user_clk => open,
        gpio_out => rgpio_s_o,
        gpio_in => rgpio_s_i,
        resetn => '1',
        ready => open
      );
    --activate
    rgpio_s_i(31 downto 28) <= rgpio_active_code;
    RGPIO_S_ENABLED <= '1' when (rgpio_s_o(31 downto 28)= rgpio_active_code) else '0';
    RGPIO_S_MASTER_ACTIVATION_CODE <= rgpio_s_o(31 downto 28);
    --data
    RGPIO_S_OUT(23 downto 0)  <= rgpio_s_o(23 downto 0);
    rgpio_s_i(23 downto 0)    <= RGPIO_S_IN(23 downto 0);
    --reserved
    s_res_1: if(C_ADD_RESERVED =1) generate
      RGPIO_S_RESERVED_OUT(3 downto 0)  <= rgpio_s_o(27 downto 24);
      rgpio_s_i(27 downto 24)    <= RGPIO_S_RESERVED_IN(3 downto 0);
    end generate; 
    s_res_0: if(C_ADD_RESERVED=0) generate
      rgpio_s_i(27 downto 24)  <= rgpio_s_o(27 downto 24);
    end generate; 
  end generate;



------------------------


---------------------------------------------------------------------

end Behavioral;
