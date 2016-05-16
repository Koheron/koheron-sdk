`timescale 1 ns / 1 ps

module averager_counter_tb();
  
  parameter FAST_COUNT_WIDTH = 5;
  parameter SLOW_COUNT_WIDTH = 10;

  reg                          restart;
  reg                          clken;
  reg [FAST_COUNT_WIDTH-1:0]   count_max;
  reg                          clk;
  reg                          avg_on;
  wire                         clr_fback;
  wire                         ready;
  wire                         wen;
  wire                         avg_on_out;
  wire [SLOW_COUNT_WIDTH-1:0]  n_avg;
  wire [FAST_COUNT_WIDTH+1:0]  address;

  averager_counter #(.FAST_COUNT_WIDTH(FAST_COUNT_WIDTH), 
                  .SLOW_COUNT_WIDTH(SLOW_COUNT_WIDTH)) 
  DUT (
    .restart(restart),
    .clken(clken),
    .count_max(count_max),
    .clk(clk),
    .avg_on(avg_on),
    .clr_fback(clr_fback),
    .ready(ready),
    .wen(wen),
    .n_avg(n_avg),
    .avg_on_out(avg_on_out),
    .address(address)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 0;
    restart = 0;
    clken = 0;
    count_max = 15;
    avg_on = 1;
    #(10*CLK_PERIOD) clken = 1;
    #(40 *CLK_PERIOD) restart = 1;
    #(1*CLK_PERIOD) restart = 0;
    #(100*CLK_PERIOD) avg_on = 0;
    #(500*CLK_PERIOD) restart = 1;
    #(1*CLK_PERIOD) restart = 0;
    #(1000*CLK_PERIOD) 
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


