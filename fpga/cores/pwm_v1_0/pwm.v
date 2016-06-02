
`timescale 1 ns / 1 ps

module pwm #
(
  parameter NBITS = 10
)
(
  input clk,
  input rst,
  input [NBITS-1:0] threshold,
  output pwm_out
);   
  reg pwm, pwm_reg;
  reg [NBITS-1:0] count, count_reg;
   
  assign pwm_out = pwm_reg;

  always @(*) begin
    count = count_reg + 1'b1;
    if (threshold > count_reg)
      pwm = 1'b1;
    else
      pwm = 1'b0;
  end

  always @(posedge clk) begin
    if (rst) begin
      count_reg <= 1'b0;
    end else begin
      count_reg <= count;
    end
    pwm_reg <= pwm;
  end

endmodule
