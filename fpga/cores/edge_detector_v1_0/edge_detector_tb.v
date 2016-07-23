`timescale 1 ns / 1 ps

module edge_detector_tb();
  
  parameter PULSE_WIDTH = 2;

  reg                          din;
  reg                          clk;
  wire                         dout;

  edge_detector #(.PULSE_WIDTH(PULSE_WIDTH)) 
  DUT (
    .din(din),
    .clk(clk),
    .dout(dout)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    $dumpfile("test.vcd");
    $dumpvars(0);
    clk = 1;
    din = 0;
    #(10*CLK_PERIOD) din = 1;
    #(10*CLK_PERIOD) din = 0;
    #(40 *CLK_PERIOD) din = 1;
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

endmodule
