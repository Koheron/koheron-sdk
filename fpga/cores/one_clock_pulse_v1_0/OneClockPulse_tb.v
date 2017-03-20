`timescale 1 ns / 1 ps

module OneClockPulse_tb();
  

  reg                       clk;
  reg                       trig;
  wire 		            pulse;
 
OneClockPulse #(
) 
DUT (
    .clk(clk),
    .trig(trig),
    .pulse(pulse)

  );

  parameter CLK_PERIOD = 5;

  initial begin
    clk = 1;
    trig = 0;
 
    #(100*CLK_PERIOD)
    trig = 1;
    #(1000*CLK_PERIOD)
    
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule
