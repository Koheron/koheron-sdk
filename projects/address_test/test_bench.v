`timescale 1 ns / 1 ps

module address_tb();

  parameter WIDTH = 8;
  
  reg clk;
  reg [31:0] cfg;
  reg [31:0] period;

  wire restart;
  wire tvalid;
  wire [WIDTH+1:0]addr;

  system_wrapper
  DUT (
    .clk(clk),
    .cfg(cfg),
    .period(period),
    .restart(restart),
    .addr(addr),
    .tvalid(tvalid)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 0;
    #(10 * CLK_PERIOD) period = 2**WIDTH - 1; 
    #(10 * CLK_PERIOD) cfg = 2'b01;
    #(10 * CLK_PERIOD) cfg = 2'b11;
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;
  
endmodule


