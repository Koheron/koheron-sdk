
`timescale 1 ns / 1 ps

module right_shifter
 #
(
  parameter TOTAL_WIDTH = 48,
  parameter DATA_WIDTH = 16,
  parameter [TOTAL_WIDTH-DATA_WIDTH-1:0] zero = 0
)
(
  input wire clk,
  input wire signed [DATA_WIDTH-1:0] in,
  output reg signed [TOTAL_WIDTH-1:0] out,
  input wire [$clog2(TOTAL_WIDTH):0] shift
);

  always @(posedge clk) begin
    out <= ($signed({in,zero}) >>> shift);
  end

endmodule
