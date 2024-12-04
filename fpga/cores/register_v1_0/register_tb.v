`timescale 1 ns / 1 ps

module register_tb();
  parameter NBITS = 16;
  

  reg                       clk;
  reg        [NBITS-1:0]    din ;
  reg                       ce;

  wire 		[NBITS-1:0]     dout;
 
register #(
) 
DUT (
    .clk(clk),
    .ce(ce),
    .din(din),
    .dout(dout)

  );

  parameter CLK_PERIOD = 10;

  initial begin
    clk = 1;
    ce = 0;
    din = 0;
 
    #(100*CLK_PERIOD)
    din = 1;
    #(100*CLK_PERIOD)
    ce = 1;
    #(1*CLK_PERIOD)
    ce = 0;
   #(100*CLK_PERIOD)
     din = 2;
   #(100*CLK_PERIOD)
      ce = 1;
      #(1*CLK_PERIOD)
      ce = 0;

    #(1000*CLK_PERIOD)
    
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule
