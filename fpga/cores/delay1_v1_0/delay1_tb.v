`timescale 1 ns / 1 ps

module delay1_tb();
  parameter NBITS = 1;
  

  reg                       clk;
  reg                       din;
  wire 		            dout;
 
delay1 #(
) 
DUT (
    .clk(clk),
    .din(din),
    .dout(dout)

  );

  parameter CLK_PERIOD = 10;

  initial begin
    clk = 1;
    din = 0;
 
    #(100*CLK_PERIOD)
    din = 1;
    #(1000*CLK_PERIOD)
    
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule
