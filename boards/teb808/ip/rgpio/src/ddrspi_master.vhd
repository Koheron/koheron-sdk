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

library ieee;

--use ieee.std_logic_1164.all;
--use ieee.numeric_std.all;

use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;


entity ddrspi_master is
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
end ddrspi_master;

architecture arch_imp of ddrspi_master is

signal sr: std_logic_vector(C_DATA_BITS-1 downto 0);

signal cnt: std_logic_vector(5 downto 0) := "000000";
signal ready_i: std_logic;
signal c_d: std_logic;
signal control: std_logic;
signal control_d: std_logic;
signal control_d2: std_logic;
signal control_d3: std_logic;

signal dbit: std_logic;

signal rx_sr: std_logic_vector(C_DATA_BITS-1 downto 0);

begin

-- output transmit clock in the middle of the data bit
process (clk)
begin
   if (falling_edge(clk)) then
        TX_CLK <= cnt(0);
   end if;
end process;

process (clk)
begin
   if (rising_edge(clk)) then
        TX_DATA <= c_d;
   end if;
end process;



	ready_i <= '1' when cnt = "111111" else '0';
	ready <= ready_i;
	
	dbit <= sr(31); 
	c_d <=  dbit when cnt(0) = '0' else control;
	
	-- set control bit 1 for DATA Latch strobe
	control <= '1' when cnt = "111111" else '0';
	

    valid <= control_d3; 
	
	
process(clk)
begin
	if (rising_edge(clk)) then
	   control_d <= control;
	   control_d2 <= control_d;
	   control_d3 <= control_d2;
	
		cnt <= cnt + "000001";
		if (ready_i = '1') then
		  sr <= data_in;
		elsif cnt(0) = '1' then
		  sr <= sr(30 downto 0) & '0';
		end if;

	   if control_d2 = '1' then
          data_out <= rx_sr;         
        end if;  
		
	end if;
end process;

process(clk)
begin
	if (falling_edge(clk)) then
	   if cnt(0) = '1' then
	     rx_sr <= rx_sr(30 downto 0) & RX_DATA;
	   end if;  
	end if;
end process;




end arch_imp;
