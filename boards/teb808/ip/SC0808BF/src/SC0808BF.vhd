----------------------------------------------------------------------------------
-- Company: Trenz Electronic GmbH
-- Engineer: John Hartfiel, Antti Lukats 
-- 
-- Create Date: 09.07.2018 
-- Design Name: 
-- Module Name: SC08808 - Behavioral
-- Project Name: 
-- Target Devices: TE0808 SoM with TEBF0808 carrier
-- Tool Versions: 2018.2
-- Description: 
-- 
-- Dependencies: 
-- 
-- Revision:
-- Revision 0.1  
-- Additional Comments:
-- 
----------------------------------------------------------------------------------


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity SC0808BF is 
    Generic (
      C_GENERIC  : integer range 0 to 3          := 1
    );
    Port 
    ( 
      PS_AUX_DI   : out  std_logic;
      PS_AUX_DO   : in  std_logic;
      PS_AUX_OE   : in  std_logic;
      PS_DP_HPD      : out  std_logic;

      -- Slave CPLD
      SC0        : out  std_logic;  -- LED_HD
      SC5        : in  std_logic;  -- RGPIO RXD
      SC6        : out  std_logic; -- RGPIO Clock
      SC7        : out  std_logic; -- RGPIO TXD

      -- Master CPLD
      SC10_I     : in  std_logic;  -- AUX IO 
      SC10_O     : out  std_logic;  -- 
      SC10_T     : out  std_logic;  -- 
      SC11       : out  std_logic;  -- AUX DIR
      SC12       : in  std_logic;  -- HPD


      SC13        : in  std_logic;  -- RGPIO RXD
      SC15        : out  std_logic; -- RGPIO Clock
      SC14        : out  std_logic; -- RGPIO TXD
      SC16        : out  std_logic; -- CAN S
      SC17        : out  std_logic; -- LED_XMOD2
      SC18        : out  std_logic; -- CAN TX
      SC19        : in  std_logic; -- CAN RX
	   
	   
	   -------------------------------------------
    --CAN
    CAN_RX    : out std_logic;
    CAN_TX    : in std_logic;
    CAN_S     : in std_logic;
    
    --LED
    LED_HD    : in std_logic;
    LED_XMOD2 : in std_logic;
    -- Master CPLD
		-- -external
		RGPIO_M_CLK   : in std_logic;  --  RGPIO Clock
		RGPIO_M_RX    : out  std_logic;   -- RGPIO RXD        
		RGPIO_M_TX    : in std_logic;   -- RGPIO TXD 
	   
    -- Slave CPLD
		-- -external
		RGPIO_S_CLK   : in std_logic;  --  RGPIO Clock
		RGPIO_S_RX    : out  std_logic;   -- RGPIO RXD        
		RGPIO_S_TX    : in std_logic   -- RGPIO TXD 
     
	);

	
	
end SC0808BF;

architecture Behavioral of SC0808BF is



begin

--CAN
    CAN_RX <= SC19;
    SC18 <= CAN_TX;
    SC16 <= CAN_S;
    
--LED
  SC0    <= LED_HD;
  SC17 <= LED_XMOD2;
    
--Audio
   PS_AUX_DI  <= SC10_I;
   SC10_O     <= PS_AUX_DO;
   SC10_T     <= PS_AUX_OE;     -- Enable as output when input OE = Low
   SC11       <= not PS_AUX_OE;
    
   PS_DP_HPD <= SC12; -- change to RGPIO ?

--
-- Master CPLD
--
  SC15 <= RGPIO_M_CLK;  --  RGPIO Clock
  RGPIO_M_RX    <= SC13;   -- RGPIO RXD        
  SC14 <=RGPIO_M_TX;   -- RGPIO TXD 
   
  -- Slave CPLD

  SC6 <= RGPIO_S_CLK;  --  RGPIO Clock
  RGPIO_S_RX    <= SC5;   -- RGPIO RXD        
  SC7 <= RGPIO_S_TX;   -- RGPIO TXD 
  

end Behavioral;
