`timescale 1 ns / 1 ps

module boxcar_filter #
(
  parameter integer DATA_WIDTH = 16
)
(
  input  wire clk,
  input  wire signed [DATA_WIDTH-1:0]  din,
  output wire signed [DATA_WIDTH-1:0]  dout
);

  reg signed [DATA_WIDTH+1-1:0] sum0;
  reg signed [DATA_WIDTH+1-1:0] sum1;
  reg signed [DATA_WIDTH+2-1:0] sum;

  reg signed [DATA_WIDTH-1:0] din0;
  reg signed [DATA_WIDTH-1:0] din1;
  reg signed [DATA_WIDTH-1:0] din2;
  reg signed [DATA_WIDTH-1:0] din3;

  always @(posedge clk) begin
    din0 <= din;
    din1 <= din0;
    din2 <= din1;
    din3 <= din2;
    sum0 <= din0 + din2;
    sum1 <= din1 + din3;
    sum <= sum0 + sum1;
  end

  assign dout = sum[DATA_WIDTH+2-1:2];

endmodule
