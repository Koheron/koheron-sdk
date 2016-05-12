`timescale 1 ns / 1 ps

module averager_counter_tb();
  
  parameter FAST_COUNT_WIDTH = 5;
  parameter SLOW_COUNT_WIDTH = 10;

  reg                          restart;
  reg                          clken;
  reg [FAST_COUNT_WIDTH-1:0]   count_max;
  reg                          clk;
  wire                         init;
  wire                         ready;
  wire                         wen;
  wire [SLOW_COUNT_WIDTH-1:0]  n_avg;
  wire [FAST_COUNT_WIDTH+1:0]  address;

  averager_counter #(.FAST_COUNT_WIDTH(FAST_COUNT_WIDTH), 
                  .SLOW_COUNT_WIDTH(SLOW_COUNT_WIDTH)) 
  DUT (
    .restart(restart),
    .clken(clken),
    .count_max(count_max),
    .clk(clk),
    .init(init),
    .ready(ready),
    .wen(wen),
    .n_avg(n_avg),
    .address(address)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 0;
    restart = 0;
    clken = 0;
    count_max = 15;
    #(10*CLK_PERIOD) clken = 1;
    #(40 *CLK_PERIOD) restart = 1;
    #(1*CLK_PERIOD) restart = 0;
    #(100*CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


