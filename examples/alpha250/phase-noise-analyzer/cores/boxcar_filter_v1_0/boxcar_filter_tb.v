`timescale 1 ns / 1 ps

module boxcar_filter_tb();

  parameter DATA_WIDTH = 8;

  reg clk;
  reg signed [DATA_WIDTH-1:0] din;
  wire signed [DATA_WIDTH-1:0] dout;


  boxcar_filter #(
    .DATA_WIDTH(DATA_WIDTH)
  )
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
    #(CLK_PERIOD) din = -1;
    #(CLK_PERIOD) din = 10;
    #(CLK_PERIOD) din = -2;
    #(CLK_PERIOD) din = -5;
    #(CLK_PERIOD) din = -6;
    #(CLK_PERIOD) din = 0;
    #(CLK_PERIOD) din = -1;
    #(CLK_PERIOD) din = 10;
    #(CLK_PERIOD) din = -2;
    #(CLK_PERIOD) din = 1;
    #(CLK_PERIOD) din = -1;
    #(CLK_PERIOD) din = 10;
    #(CLK_PERIOD) din = -2;
    #(CLK_PERIOD) din = -5;
    #(CLK_PERIOD) din = -6;
    #(CLK_PERIOD) din = 0;
    #(CLK_PERIOD) din = -1;
    #(CLK_PERIOD) din = 10;
    #(CLK_PERIOD) din = -2;
    #(CLK_PERIOD) din = 1;
    #(CLK_PERIOD) din = -1;
    #(CLK_PERIOD) din = 10;
    #(CLK_PERIOD) din = -2;
    #(CLK_PERIOD) din = -5;
    #(CLK_PERIOD) din = -6;
    #(CLK_PERIOD) din = 0;
    #(CLK_PERIOD) din = -1;
    #(CLK_PERIOD) din = 10;
    #(CLK_PERIOD) din = -2;
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


