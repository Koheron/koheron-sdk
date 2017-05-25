`timescale 1 ns / 1 ps

module vhdl_counter_tb();

  reg clk;
  reg reset;
  reg enable;
  wire [4-1:0] count;

  vhdl_counter #()
  DUT (
    .clk(clk),
    .reset(reset),
    .enable(enable),
    .count(count)
  );

  parameter CLK_PERIOD = 8;

  always #(CLK_PERIOD/2) clk = ~clk;

  initial begin
    clk = 1'b1;
    reset = 0;
    enable = 0;
    #(CLK_PERIOD) reset = 1;
    #(CLK_PERIOD) reset = 0;
    #(10*CLK_PERIOD)
    enable = 1;
    #(100000*CLK_PERIOD)
    $finish;
  end

endmodule
