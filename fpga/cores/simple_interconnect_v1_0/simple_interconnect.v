`timescale 1 ns / 1 ps

module simple_interconnect #
(
  parameter integer WIDTH = 32,
  parameter integer N_INPUTS = 3,
  parameter integer SEL_WIDTH = 2
)
(
  input  wire                          clk,
  input  wire [(N_INPUTS * WIDTH)-1:0] in,
  input  wire [SEL_WIDTH-1 :0]         sel,
  output reg  [WIDTH-1:0]              out
);

  reg [SEL_WIDTH-1 :0] sel_reg;

  always @(posedge clk) begin
    sel_reg <= sel;
  end

  // http://stackoverflow.com/questions/25123924/verilog-range-must-be-bounded-by-constant-expression
  always @(posedge clk) begin
    out <= in[sel_reg * WIDTH +: WIDTH];
  end

endmodule


