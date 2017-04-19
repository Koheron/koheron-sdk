`timescale 1 ns / 1 ps

module comparator #
(
  parameter integer DATA_WIDTH = 16,
  parameter OPERATION = "EQ"
)
(
  input  wire [DATA_WIDTH-1:0]  a,
  input  wire [DATA_WIDTH-1:0]  b,
  output wire dout              // a greater or equal than b     
);

  generate
    if (OPERATION == "GE")
    begin : GE
      assign dout = (a >= b);
    end
    if (OPERATION == "GT")
    begin : GT
      assign dout = (a > b);
    end
    if (OPERATION == "LE")
    begin : LE
      assign dout = (a <= b);
    end
    if (OPERATION == "LT")
    begin : LT
      assign dout = (a < b);
    end
    if (OPERATION == "EQ")
    begin : EQ
      assign dout = (a == b);
    end
    if (OPERATION == "NE")
    begin : NE
      assign dout = (a != b);
    end
  endgenerate
endmodule
