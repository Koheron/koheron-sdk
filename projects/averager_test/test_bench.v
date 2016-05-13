`timescale 1 ns / 1 ps

module averager_tb();

  parameter WIDTH = 8;
  
  reg avg_off;
  reg clk;
  reg [13:0]din;
  reg [WIDTH-1:0]period;
  reg restart;
  reg [WIDTH-1:0]threshold;
  reg tvalid;

  wire [3:0]wen;
  wire [31:0]dout;
  wire [32-WIDTH-1:0] n_avg;
  wire ready;
  wire [WIDTH+1:0]addr;

  system_wrapper
  DUT (
    .addr(addr),
    .avg_off(avg_off),
    .clk(clk),
    .din(din),
    .dout(dout),
    .n_avg(n_avg),
    .period(period),
    .ready(ready),
    .restart(restart),
    .threshold(threshold),
    .tvalid(tvalid),
    .wen(wen)  
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 0;
    avg_off = 0;
    din = 0;
    restart = 0;
    tvalid = 0;
    period = 0;
    threshold = 0;
    #(CLK_PERIOD * 2**(WIDTH-1))
    period = 2**WIDTH - 1;
    threshold = 2**WIDTH - 6;
    #(CLK_PERIOD * 2**(WIDTH-1)) tvalid = 1;

    #(50*CLK_PERIOD) restart = 1;
    #(CLK_PERIOD) restart = 0;
    #(10 * CLK_PERIOD) avg_off = 1;
 
    #(2000 * CLK_PERIOD) restart = 1;
    #(CLK_PERIOD) restart = 0;
    #(10 * CLK_PERIOD) avg_off = 0;

    #(2000 * CLK_PERIOD) restart = 1;
    #(CLK_PERIOD) restart = 0;

    #(2000 * CLK_PERIOD) restart = 1;
    #(CLK_PERIOD) restart = 0;

    #(100000*CLK_PERIOD)
    
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;
  
  always begin
    #(CLK_PERIOD * 2**(WIDTH-1)) din = 1;
    #(CLK_PERIOD * 2**(WIDTH-1)) din = 0;
  end   

endmodule


