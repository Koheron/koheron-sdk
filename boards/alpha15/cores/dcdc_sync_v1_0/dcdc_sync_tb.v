`timescale 1 ns / 1 ps

module dcdc_sync_tb();

  reg clk;
  reg en;
  reg state_out;
  wire dcdc_clk;

  parameter CLK_PERIOD = 5;
  parameter DIVIDER = 10;

  dcdc_sync #(.DIVIDER(DIVIDER))
  
  DUT (
    .clk(clk),
    .en(en),
    .state_out(state_out),
    .dcdc_clk(dcdc_clk)
  );

  initial begin
    clk = 1;
    en = 1;
    state_out = 0;
    #(100*CLK_PERIOD)
    en = 0;
    #(100*CLK_PERIOD)
    state_out = 1;
    #(100*CLK_PERIOD)
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;
endmodule