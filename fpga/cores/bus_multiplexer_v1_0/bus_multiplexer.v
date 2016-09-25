`timescale 1 ns / 1 ps

module bus_multiplexer #
(
  parameter integer WIDTH = 32
)
(
  input  wire [WIDTH-1:0] din0,
  input  wire [WIDTH-1:0] din1,
  input  wire             sel,
  output wire [WIDTH-1:0] dout
);

  assign dout = (sel) ? din1 : din0;

endmodule
