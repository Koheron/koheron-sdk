`timescale 1 ns / 1 ps

module mmcm_phase_shifter_tb();

  reg clk;
  reg [31:0] ctl;
  wire psen;
  wire psincdec;

  parameter CLK_PERIOD = 5;

  mmcm_phase_shifter #()

  DUT (
    .clk(clk),
    .ctl(ctl),
    .psen(psen),
    .psincdec(psincdec),
    .ready(ready)
  );

  initial begin
    clk = 1;
    ctl = 0;
    #(10*CLK_PERIOD)
    ctl = 16'b0000_0100_1100;
    #(10*CLK_PERIOD)
    ctl = 16'b0000_0100_1000;
    #(100*CLK_PERIOD)
    ctl = 16'b0000_0001_0100;
    #(100*CLK_PERIOD)
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;
endmodule