`timescale 1 ns / 1 ps

module ad7124_tb();

  reg           clk;
  reg           resetn;
  reg           sdo;
  wire          sclk;
  wire          cs;
  wire          sdi;
  wire [24-1:0] dout;
  wire          valid;

  ad7124 DUT (
    .clk(clk),
    .resetn(resetn),
    .sdo(sdo),
    .sclk(sclk),
    .cs(cs),
    .sdi(sdi),
    .dout(dout),
    .valid(valid)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    sdo = 1;
    resetn = 0;
    #(100*CLK_PERIOD)
    resetn = 1;
    #(20000*CLK_PERIOD)
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule
