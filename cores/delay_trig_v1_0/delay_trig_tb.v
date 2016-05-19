`timescale 1 ns / 1 ps

module delay_trig_tb();
  
  reg clk;
  reg trig_in;
  reg valid;
  wire trig_out;
  wire ready;

  delay_trig #() 
  DUT (
    .clk(clk),
    .trig_in(trig_in),
    .valid(valid),
    .trig_out(trig_out),
    .ready(ready)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 0;
    trig_in = 0;
    valid = 0;
    #(10*CLK_PERIOD) trig_in = 1;
    #(CLK_PERIOD) trig_in = 0;
    #(10*CLK_PERIOD) valid = 1;
    #(100*CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


