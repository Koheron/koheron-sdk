`timescale 1 ns / 1 ps

module latched_mux #
(
  parameter integer WIDTH = 32,
  parameter integer N_INPUTS = 3,
  parameter integer SEL_WIDTH = 2
)
(
  input  wire                          clk,
  input  wire                          clken,
  input  wire [(N_INPUTS * WIDTH)-1:0] din,
  input  wire [SEL_WIDTH-1 :0]         sel,
  output reg  [WIDTH-1:0]              dout
);

  reg [SEL_WIDTH-1 :0] sel_reg;
  initial sel_reg = {(SEL_WIDTH){1'b0}};

  always @(posedge clk) begin
    if (clken == 1'b1) begin
       sel_reg <= sel;
    end
  end

  // http://stackoverflow.com/questions/25123924/verilog-range-must-be-bounded-by-constant-expression
  always @(posedge clk) begin
    dout <= din[sel_reg * WIDTH +: WIDTH];
  end

endmodule


