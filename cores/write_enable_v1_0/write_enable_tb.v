`timescale 1 ns / 1 ps

module write_enable_tb();
  
  parameter BRAM_WIDTH = 5;

  reg                   restart;
  reg                   end_cycle;
  reg  [BRAM_WIDTH-1:0] count_max;
  reg                   clk;
  wire                  wen;
  wire [BRAM_WIDTH-1:0] count;
  wire                  init;
  wire                  ready;

  write_enable #(.BRAM_WIDTH(BRAM_WIDTH)) DUT (
    .restart(restart),
    .end_cycle(end_cycle),
    .count_max(count_max),
    .clk(clk),
    .wen(wen),
    .count(count),
    .init(init),
    .ready(ready)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    restart = 0;
    end_cycle = 0;
    count_max = 31;
    #(10*CLK_PERIOD) restart = 1;
    #(1*CLK_PERIOD) restart = 0;
    #(5*CLK_PERIOD) end_cycle = 1;
    #(CLK_PERIOD) end_cycle = 0;
    #(100*CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


