`timescale 1 ns / 1 ps

module cycle_counter_tb();
  
  parameter FAST_COUNT_WIDTH = 5;
  parameter SLOW_COUNT_WIDTH = 10;

  reg                          sclr;
  reg                          clken;
  reg [FAST_COUNT_WIDTH-1:0]   count_max;
  reg                          clk;
  wire                         end_cycle;
  wire [FAST_COUNT_WIDTH-1:0]  fast_count;
  wire [FAST_COUNT_WIDTH-1:0]  slow_count;

  cycle_counter #(.FAST_COUNT_WIDTH(FAST_COUNT_WIDTH), 
                  .SLOW_COUNT_WIDTH(SLOW_COUNT_WIDTH)) 
  DUT (
    .sclr(sclr),
    .clken(clken),
    .count_max(count_max),
    .clk(clk),
    .end_cycle(end_cycle),
    .fast_count(fast_count),
    .slow_count(slow_count)    
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    clken = 1;
    count_max = 63;
    count_max = 15;
    #(10*CLK_PERIOD) sclr = 1;
    #(1*CLK_PERIOD) sclr = 0;
    #(100*CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


