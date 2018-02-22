// Pulse density Modulator
// http://www.fpga4fun.com/PWM_DAC_2.html

`timescale 1 ns / 1 ps

module pdm #
(
  parameter NBITS = 11
)
(
  input wire                      clk,
  input wire [NBITS-1:0]          din,
  input wire                      rst,

  output wire                     dout
);

  reg [NBITS+1-1:0] accumulator;

  always @(posedge clk) begin
    if (rst) begin
      accumulator <= 0;
    end else begin
      accumulator <= accumulator[NBITS-1:0] + din;
    end
  end

  assign dout = accumulator[NBITS];

endmodule
