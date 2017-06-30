`timescale 1 ns / 1 ps

module saturation_tb();

  parameter DATA_WIDTH = 8;
  parameter MAX_VAL = 15;

  reg clk;
  reg signed [DATA_WIDTH-1:0] din;
  wire signed [DATA_WIDTH-1:0] dout;


  saturation #(
    .DATA_WIDTH(DATA_WIDTH),
    .MAX_VAL(MAX_VAL))
  DUT (
    .clk(clk),
    .din(din),
    .dout(dout)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    din = 0;
    #(CLK_PERIOD) din = 1;
    #(10 * CLK_PERIOD) din = 18;
    #(CLK_PERIOD) din = 1;
    #(CLK_PERIOD) din = 0;
    #(CLK_PERIOD) din = -1;
    #(CLK_PERIOD) din = -2;
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


