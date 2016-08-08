`timescale 1 ns / 1 ps

module pulse_generator_tb();

  parameter integer PULSE_WIDTH_WIDTH = 8;
  parameter integer PULSE_PERIOD_WIDTH = 16;

  reg clk;
  reg [PULSE_WIDTH_WIDTH-1:0] pulse_width;
  reg [PULSE_PERIOD_WIDTH-1:0] pulse_period;
  reg rst;
  wire dout;
  wire [PULSE_PERIOD_WIDTH-1:0] cnt;

  pulse_generator #(
    .PULSE_WIDTH_WIDTH(PULSE_WIDTH_WIDTH),
    .PULSE_PERIOD_WIDTH(PULSE_PERIOD_WIDTH)
  )
  DUT (
    .clk(clk),
    .pulse_width(pulse_width),
    .pulse_period(pulse_period),
    .rst(rst),
    .dout(dout),
    .cnt(cnt)
  );

  parameter CLK_PERIOD = 8;

  always #(CLK_PERIOD/2) clk = ~clk;
    
  initial begin
    clk = 1'b1;
    pulse_width = 10;
    pulse_period = 100;
    rst = 0;
    #(166*CLK_PERIOD)
    rst = 1;
    #(CLK_PERIOD)
    rst = 0;
    #(1000*CLK_PERIOD)
    pulse_period = 50;
    pulse_width = 25;
    #(100000*CLK_PERIOD)
    $finish;
  end

endmodule
