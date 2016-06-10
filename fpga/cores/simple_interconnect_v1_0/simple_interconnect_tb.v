`timescale 1 ns / 1 ps

module simple_interconnect_tb();
  
  parameter WIDTH = 8;
  parameter N_INPUTS = 3;
  parameter SEL_WIDTH = 2;

  reg clk;
  reg [(N_INPUTS * WIDTH)-1:0] in;
  reg [SEL_WIDTH-1 :0] sel;
  wire[WIDTH-1:0] out;

  simple_interconnect #(
    .WIDTH(WIDTH),
    .N_INPUTS(N_INPUTS),
    .SEL_WIDTH(SEL_WIDTH)) 
  DUT (
    .clk(clk),
    .in(in),
    .sel(sel),
    .out(out)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    in = {{(WIDTH){1'b0}},{(WIDTH){1'b1}},{(WIDTH){1'b0}}};
    sel = 2'b00;
    #(100*CLK_PERIOD)
    sel = 2'b01;
    #(100*CLK_PERIOD)
    sel = 2'b11;
    #(100*CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;
  
endmodule


