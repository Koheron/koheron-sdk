----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 03.08.2016 14:53:24
-- Design Name: 
-- Module Name: ddsrpi_slave - Behavioral
-- Project Name: 
-- Target Devices: 
-- Tool Versions: 
-- Description: 
-- 
-- Dependencies: 
-- 
-- Revision:
-- Revision 0.01 - File Created
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

entity ddsrpi_slave is
    Port ( RX_CLK : in STD_LOGIC;
           RX_DATA : in STD_LOGIC;
           TX_DATA : out STD_LOGIC;
           user_clk : out STD_LOGIC;
           gpio_out : out STD_LOGIC_VECTOR (31 downto 0);
           gpio_in : in STD_LOGIC_VECTOR (31 downto 0);
           resetn : in STD_LOGIC;
           ready : out STD_LOGIC);
end ddsrpi_slave;

architecture Behavioral of ddsrpi_slave is

signal rx_sr: std_logic_vector(31 downto 0);
signal tx_sr: std_logic_vector(31 downto 0);
signal gpio_out_s: std_logic_vector(31 downto 0);

signal clk : std_logic;
signal sync : std_logic;

begin
    ready <= sync;
    gpio_out <= gpio_out_s;
    
    clk <= RX_CLK;

process (clk)
begin
   if (rising_edge(clk)) then
        rx_sr <= rx_sr(30 downto 0) & RX_DATA;
        if sync='1' then
            gpio_out_s <= rx_sr;        
        end if;    
   end if;
end process;

TX_DATA <= tx_sr(31);

process (clk)
begin
   if (falling_edge(clk)) then
        sync <= RX_DATA;

        
        if sync='1' then
            tx_sr <= gpio_in;
        else
            tx_sr <= tx_sr(30 downto 0) & '0';
                    
        end if;    
        
   end if;
end process;


end Behavioral;
