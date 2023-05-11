----------------------------------------------------------------------------------
-- Company: Trenz Electronic GmbH
-- Engineer: Oleksandr Kiyenko
----------------------------------------------------------------------------------
library ieee;
use ieee.STD_LOGIC_1164.all;
use ieee.numeric_std.all;
----------------------------------------------------------------------------------
entity axis_live_audio_v1_0 is
port (
	axis_aclk				: in  STD_LOGIC;
	
	s_axis_tdata			: in  STD_LOGIC_VECTOR(31 downto 0);
	s_axis_tid				: in  STD_LOGIC;
	s_axis_tready			: out STD_LOGIC;
	s_axis_tvalid			: in  STD_LOGIC;

	m_axis_tdata			: out STD_LOGIC_VECTOR(31 downto 0);
	m_axis_tid				: out STD_LOGIC;
	m_axis_tready			: in  STD_LOGIC;
	m_axis_tvalid			: out STD_LOGIC;
	
	-- Serial input/output Interface I2S
	BCLK					: in  STD_LOGIC;
	LRCLK					: in  STD_LOGIC;
	DAC_SDATA				: out STD_LOGIC;
	ADC_SDATA				: in  STD_LOGIC
);
end axis_live_audio_v1_0;
----------------------------------------------------------------------------------
architecture arch_imp of axis_live_audio_v1_0 is
---------------------------------------------------------------------------------
constant C_CYCLES_PER_FRAME		: INTEGER	:= 32;
-- Receiver
signal left_audio_data			: STD_LOGIC_VECTOR(23 downto 0);
signal right_audio_data			: STD_LOGIC_VECTOR(23 downto 0);
-- Transmitter
signal transmit_data			: STD_LOGIC_VECTOR(C_CYCLES_PER_FRAME - 1 downto 0);
signal receive_data 			: STD_LOGIC_VECTOR(C_CYCLES_PER_FRAME - 1 downto 0);
signal bclk_sr                  : STD_LOGIC_VECTOR(1 downto 0);
signal lrclk_sr                 : STD_LOGIC_VECTOR(1 downto 0);
signal parity                   : STD_LOGIC;
----------------------------------------------------------------------------------
begin
----------------------------------------------------------------------------------
-- Receiver
s_axis_tready			<= '1';
process(axis_aclk)
begin
	if(axis_aclk = '1' and axis_aclk'event)then
		if(s_axis_tvalid = '1')then
			if(s_axis_tid = '0')then
				left_audio_data		<= s_axis_tdata(27 downto 4);
			else
				right_audio_data	<= s_axis_tdata(27 downto 4);
			end if;
		end if;
	end if;
end process;
----------------------------------------------------------------------------------
-- Transmitter
process(axis_aclk)
begin
	if(axis_aclk = '1' and axis_aclk'event)then
        bclk_sr     <= bclk_sr(0) & BCLK;
        if(bclk_sr = "10")then      -- BCLK Falling edge
            if(lrclk_sr(1) /= lrclk_sr(0))then  -- LRCLK edge
                if(lrclk_sr(0) = '0')then
                    transmit_data	<= left_audio_data & STD_LOGIC_VECTOR(TO_UNSIGNED(0,(C_CYCLES_PER_FRAME - (24 + 1)))) & "0";
                else
                    transmit_data	<= right_audio_data & STD_LOGIC_VECTOR(TO_UNSIGNED(0,(C_CYCLES_PER_FRAME - (24 + 1)))) & "0";
                end if;
            else
                transmit_data	    <= transmit_data(C_CYCLES_PER_FRAME - 2 downto 0) & '0';
            end if;
        end if;
        if(bclk_sr = "01")then      -- BCLK Rising edge
            if(lrclk_sr(0) /= LRCLK)then
               m_axis_tid       <= lrclk_sr(0);
               m_axis_tvalid    <= '1';
               m_axis_tdata(31)             <= parity; -- Parity;
               m_axis_tdata(30)             <= '1';    -- Channel status
               m_axis_tdata(29)             <= '0';    -- User bit
               m_axis_tdata(28)             <= '1';    -- Validity bit
               m_axis_tdata(27 downto 4)    <= receive_data(31 downto 8);
               m_axis_tdata( 3 downto 0)    <= "0010";
            end if;
            lrclk_sr            <= lrclk_sr(0) & LRCLK;
            receive_data        <= receive_data(C_CYCLES_PER_FRAME - 2 downto 0) & ADC_SDATA;       
        else
            m_axis_tvalid    <= '0';
        end if;
	end if;
end process;

parity      <= receive_data(31) xor receive_data(30) xor receive_data(29) xor receive_data(28) xor
               receive_data(27) xor receive_data(26) xor receive_data(25) xor receive_data(24) xor
               receive_data(23) xor receive_data(22) xor receive_data(21) xor receive_data(20) xor
               receive_data(19) xor receive_data(18) xor receive_data(17) xor receive_data(16) xor
               receive_data(15) xor receive_data(14) xor receive_data(13) xor receive_data(12) xor
               receive_data(11) xor receive_data(10) xor receive_data( 9) xor receive_data( 8);

DAC_SDATA   <= transmit_data(C_CYCLES_PER_FRAME - 1);
----------------------------------------------------------------------------------
end arch_imp;
