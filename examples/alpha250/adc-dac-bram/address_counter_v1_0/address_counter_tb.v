`timescale 1 ns / 1 ps

module address_counter_tb();

  parameter COUNT_WIDTH = 5;

  reg clken;
  reg trig;
  reg clk;
  wire [31:0] address;
  wire [3:0] wen;

  address_counter #(.COUNT_WIDTH(COUNT_WIDTH))
  DUT (
    .clken(clken),
    .trig(trig),
    .clk(clk),
    .address(address),
    .wen(wen)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    clken = 1;
    trig = 0;
    #(10*CLK_PERIOD) trig = 1;
    #(1*CLK_PERIOD) trig = 0;
    #(100*CLK_PERIOD)
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


