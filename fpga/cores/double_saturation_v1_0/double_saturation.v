`timescale 1 ns / 1 ps

module double_saturation #
(
  parameter integer DATA_WIDTH = 16,
  parameter integer MAX_VAL = 10
)
(
  input  wire clk,
  input  wire signed [DATA_WIDTH-1:0]  din,
  output reg signed [DATA_WIDTH-1:0]  dout
);

  wire signed [DATA_WIDTH-1:0] din_pos;

  assign din_pos = (din > -MAX_VAL) ? din : -MAX_VAL;

  always @(posedge clk) begin
    dout <= (din > MAX_VAL) ? MAX_VAL : din_pos;
  end

endmodule
