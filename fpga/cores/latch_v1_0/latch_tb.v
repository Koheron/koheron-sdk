`timescale 1 ns / 1 ps

module latch_tb();
  parameter NBITS = 1;
  

  reg                       clk;
  reg                       set;
  reg                       reset;

  wire 		            q;
 
latch #(
) 
DUT (
    .clk(clk),
    .set(set),
    .reset(reset),
    .q(q)

  );

  parameter CLK_PERIOD = 40;

  initial begin
    clk = 1;
    set = 0;
    reset = 0;
 
    #(100*CLK_PERIOD)
    set = 1;
    #(1000*CLK_PERIOD)
    
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule
