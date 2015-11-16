/**
 * $Id: red_pitaya_analog.v 964 2014-01-24 12:58:17Z matej.oblak $
 *
 * @brief Red Pitaya analog module. Connects to ADC & DAC pins.
 *
 * @Author Matej Oblak
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in Verilog hardware description language (HDL).
 * Please visit http://en.wikipedia.org/wiki/Verilog
 * for more details on the language used herein.
 */



/**
 * GENERAL DESCRIPTION:
 *
 * Interace module between fast ADC and DAC IC.  
 *
 *
 *                 /------------\      
 *   ADC DAT ----> | RAW -> 2's | ----> ADC DATA TO USER
 *                 \------------/
 *                       ^
 *                       |
 *                    /-----\
 *   ADC CLK -------> | PLL |
 *                    \-----/
 *                       |
 *                       Ë‡
 *                 /------------\
 *   DAC DAT <---- | RAW <- 2's | <---- DAC DATA FROM USER
 *                 \------------/
 *                       |
 *                       Ë‡
 *                   /-------\
 *   DAC PWM <------ |  PWM  | <------- SLOW DAC DATA FROM USER
 *                   \-------/
 *
 *
 * ADC clock is used for main clock domain, from this double clock is made which
 * is used for driving DAC IC (using DDR transfer) and PWM counters.
 *
 * ADC interface gives unsigned number format with negative slope because 
 * input amplifier. This is transfomed into 2's complement wich is more usable in
 * digital world.
 *
 * For sending data to DAC values has to be first translated from 2's format to 
 * unsigned format, where negative output amplifier gain is taken into account.
 * Interface to DAC is DDR, positive edge used for CHA and negative for CHB.
 
 * PWM in created with counter running on 2xDAC clock. Each 16 cycles of PWM_FULL
 * counts new value is taken. Upper 8 bits are used for dac_pwm_vcnt which defines
 * PWM rate of output. This repeates 16x times, where lower 16bits of input data
 * defines if ration of dac_pwm_vcnt is one cycle more.
 * 
 */


`timescale 1 ns / 1 ps

module rp_adc_dac #
(
)
(
  // ADC IC
  input    [ 14-1: 0] adc_dat_a_i        ,  //!< ADC IC CHA data connection
  input    [ 14-1: 0] adc_dat_b_i        ,  //!< ADC IC CHB data connection
  input               adc_clk            ,
  output [ 2-1: 0]    adc_clk_source     ,  // optional ADC clock source
  //output              adc_enc_p_o	 ,
  //output              adc_enc_n_o	 ,
  output              adc_cdcs_o         ,  // ADC clock duty cycle stabilizer

  
  input               dac_clk_1x         ,
  input               dac_clk_2x         ,  
  input               dac_clk_2p         ,
  input               dac_locked         ,

  // DAC IC
  output   [ 14-1: 0] dac_dat_o          ,  //!< DAC IC combined data
  output              dac_wrt_o          ,  //!< DAC IC write enable
  output              dac_sel_o          ,  //!< DAC IC channel select
  output              dac_clk_o          ,  //!< DAC IC clock
  output              dac_rst_o          ,  //!< DAC IC reset
  
  // PWM DAC
  //output   [  4-1: 0] dac_pwm_o          ,  //!< DAC PWM - driving RC
  
  // user interface
  output   [ 14-1: 0] adc_dat_a_o        ,  //!< ADC CHA data
  output   [ 14-1: 0] adc_dat_b_o        ,  //!< ADC CHB data
  input               adc_rst_i          ,  //!< ADC reset - active low

  input    [ 14-1: 0] dac_dat_a_i        ,  //!< DAC CHA data
  input    [ 14-1: 0] dac_dat_b_i           //!< DAC CHB data

);
//---------------------------------------------------------------------------------
//
//  Analog peripherials (red_pitaya_top.v)

// ADC clock duty cycle stabilizer is enabled
assign adc_cdcs_o = 1'b1 ;

// generating ADC clock is disabled
assign adc_clk_source = 2'b10;
//assign adc_enc_p_o = 1'b1;
//assign adc_enc_n_o = 1'b0;

//---------------------------------------------------------------------------------
//
//  ADC input registers

reg  [14-1: 0] adc_dat_a  ;
reg  [14-1: 0] adc_dat_b  ;

always @(posedge adc_clk) begin
   adc_dat_a <= adc_dat_a_i[14-1:0]; // lowest 2 bits reserved for 16bit ADC
   adc_dat_b <= adc_dat_b_i[14-1:0];
end
    
assign adc_dat_a_o = {adc_dat_a[14-1], ~adc_dat_a[14-2:0]}; // transform into 2's complement (negative slope)
assign adc_dat_b_o = {adc_dat_b[14-1], ~adc_dat_b[14-2:0]};

//---------------------------------------------------------------------------------
//
//  Fast DAC - DDR interface

reg   dac_rst         ;

reg  [14-1: 0] dac_dat_a  ;
reg  [14-1: 0] dac_dat_b  ;

// output registers + signed to unsigned (also to negative slope)
always @(posedge dac_clk) begin
   dac_dat_a <= {dac_dat_a_i[14-1], ~dac_dat_a_i[14-2:0]};
   dac_dat_b <= {dac_dat_b_i[14-1], ~dac_dat_b_i[14-2:0]};
   dac_rst   <= !dac_locked;
end

ODDR oddr_dac_clk          (.Q(dac_clk_o), .D1(1'b0     ), .D2(1'b1     ), .C(dac_clk_2p), .CE(1'b1), .R(1'b0   ), .S(1'b0));
ODDR oddr_dac_wrt          (.Q(dac_wrt_o), .D1(1'b0     ), .D2(1'b1     ), .C(dac_clk_2x), .CE(1'b1), .R(1'b0   ), .S(1'b0));
ODDR oddr_dac_sel          (.Q(dac_sel_o), .D1(1'b1     ), .D2(1'b0     ), .C(dac_clk_1x), .CE(1'b1), .R(dac_rst), .S(1'b0));
ODDR oddr_dac_rst          (.Q(dac_rst_o), .D1(dac_rst  ), .D2(dac_rst  ), .C(dac_clk_1x), .CE(1'b1), .R(1'b0   ), .S(1'b0));

ODDR oddr_dac_dat [14-1:0] (.Q(dac_dat_o), .D1(dac_dat_b), .D2(dac_dat_a), .C(dac_clk), .CE(1'b1), .R(dac_rst), .S(1'b0));

endmodule

