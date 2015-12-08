`timescale 1 ns / 1 ps

module bus_multiplexer #
(
  parameter integer WIDTH = 32
)
(
  input  wire [WIDTH-1:0] in0,
  input  wire [WIDTH-1:0] in1,
  input  wire             sel,
  output wire [WIDTH-1:0] out
);

  assign out = (sel) ? in1 : in0;

endmodule


