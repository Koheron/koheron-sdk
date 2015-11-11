
`timescale 1 ns / 1 ps

module pwm #
(
  parameter PERIOD = 256
)
(
  input clk,
  input rst,
  input [PERIOD-1:0] threshold,
  output pwm
);   
  reg pwm, pwm_reg;
  reg [PERIOD-1:0] count, count_reg;
   
  assign pwm = pwm_reg;

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
