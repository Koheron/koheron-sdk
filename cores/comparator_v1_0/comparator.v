`timescale 1 ns / 1 ps

module comparator #
(
  parameter integer DATA_WIDTH = 16,
  parameter OPERATION = "EQ"
)
(
  input  wire [DATA_WIDTH-1:0]  a,
  input  wire [DATA_WIDTH-1:0]  b,
  output wire out  
);

  generate
    if (OPERATION == "GE")
    begin : GE
      assign out = (a >= b);
    end
    if (OPERATION == "GT")
    begin : GE
      assign out = (a > b);
    end
    if (OPERATION == "LE")
    begin : LE
      assign out = (a <= b);
    end
    if (OPERATION == "LT")
    begin : LE
      assign out = (a < b);
    end
    if (OPERATION == "EQ")
    begin : EQ
      assign out = (a == b);
    end
    if (OPERATION == "NE")
    begin : NEQ
      assign out = (a != b);
    end
  endgenerate
endmodule
