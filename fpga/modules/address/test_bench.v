`timescale 1 ns / 1 ps

module address_tb();

  parameter WIDTH = 8;
  
  reg clk;
  reg [31:0] cfg;
  reg [31:0] period0;

  wire restart;
  wire tvalid;
  wire [WIDTH+1:0]addr0;

  system_wrapper
  DUT (
    .clk(clk),
    .cfg(cfg),
    .period0(period0),
    .restart(restart),
    .addr0(addr0),
    .tvalid(tvalid)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 0;
    cfg = 2'b00;
    #(10 * CLK_PERIOD) period0 = 2**WIDTH - 1;
    #(10 * CLK_PERIOD) cfg = 2'b01;
    #(10 * CLK_PERIOD) cfg = 2'b11;
    #(10000 * CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;
  
endmodule


