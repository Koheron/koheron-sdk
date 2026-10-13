`timescale 1 ns / 1 ps

module rp_adc #
(
)
(
  // ADC IC
  input    [ 14-1: 0] adc_dat_a_i        ,  //!< ADC IC CHA data connection
  input    [ 14-1: 0] adc_dat_b_i        ,  //!< ADC IC CHB data connection
  input               adc_clk            ,
  output [ 2-1: 0]    adc_clk_source     ,  // optional ADC clock source
  output              adc_cdcs_o         ,  // ADC clock duty cycle stabilizer
 
  // user interface
  output   [ 14-1: 0] adc_dat_a_o        ,  //!< ADC CHA data
  output   [ 14-1: 0] adc_dat_b_o        ,  //!< ADC CHB data
  input               adc_rst_i             //!< ADC reset - active low
);
  // ADC clock duty cycle stabilizer is enabled
  assign adc_cdcs_o = 1'b1 ;

  // generating ADC clock is disabled
  assign adc_clk_source = 2'b10;

  reg  [14-1: 0] adc_dat_a  ;
  reg  [14-1: 0] adc_dat_b  ;

  always @(posedge adc_clk) begin
     adc_dat_a <= adc_dat_a_i[14-1:0]; // lowest 2 bits reserved for 16bit ADC
     adc_dat_b <= adc_dat_b_i[14-1:0];
  end
    
  assign adc_dat_a_o = {adc_dat_a[14-1], ~adc_dat_a[14-2:0]}; // transform into 2's complement (negative slope)
  assign adc_dat_b_o = {adc_dat_b[14-1], ~adc_dat_b[14-2:0]};

endmodule

