`timescale 1 ns / 1 ps

module latched_mux_tb();
  
  parameter WIDTH = 8;
  parameter N_INPUTS = 3;
  parameter SEL_WIDTH = 2;

  reg                          clk;
  reg                          clken;
  reg [(N_INPUTS * WIDTH)-1:0] in;
  reg [SEL_WIDTH-1 :0]         sel;
  wire[WIDTH-1:0]              out;

  latched_mux #(
    .WIDTH(WIDTH),
    .N_INPUTS(N_INPUTS),
    .SEL_WIDTH(SEL_WIDTH)) 
  DUT (
    .clk(clk),
    .clken(clken),
    .in(in),
    .sel(sel),
    .out(out)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    clken = 0;
    in = {{(WIDTH){1'b0}},{(WIDTH){1'b1}},{(WIDTH){1'b0}}};
    sel = 0;
    #(100*CLK_PERIOD)
    sel = 1;
    #(100*CLK_PERIOD)
    clken = 1;
    #(100*CLK_PERIOD)
    sel = 2;
    #(100*CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;
  
endmodule


