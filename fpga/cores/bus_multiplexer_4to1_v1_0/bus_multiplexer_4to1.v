`timescale 1 ns / 1 ps

module bus_multiplexer_4to1 #
(
  parameter integer WIDTH = 32
)
(
  input  wire [WIDTH-1:0] din0,
  input  wire [WIDTH-1:0] din1,
  input  wire [WIDTH-1:0] din2,
  input  wire [WIDTH-1:0] din3,
  input  wire [1:0]       sel,
  output wire [WIDTH-1:0] dout
);

  assign dout = (sel[1]) ? ((sel[0]) ? din3 : din2) : ((sel[0]) ? din1 : din0);

endmodule
