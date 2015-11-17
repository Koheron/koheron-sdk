`timescale 1 ns / 1 ps

module comparator #
(
  parameter integer DATA_WIDTH = 16
)
(
  input  wire [DATA_WIDTH-1:0]  a,
  input  wire [DATA_WIDTH-1:0]  b,
  output wire a_geq_b              // a greater or equal than b     
);
  assign a_geq_b = (a >= b);  
endmodule

