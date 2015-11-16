`timescale 1 ns / 1 ps

module rp_dac #
(
)
(
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

  input    [ 14-1: 0] dac_dat_a_i        ,  //!< DAC CHA data
  input    [ 14-1: 0] dac_dat_b_i           //!< DAC CHB data

);

  reg   dac_rst         ;

  reg  [14-1: 0] dac_dat_a  ;
  reg  [14-1: 0] dac_dat_b  ;

  // output registers + signed to unsigned (also to negative slope)
  always @(posedge dac_clk1x) begin
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

