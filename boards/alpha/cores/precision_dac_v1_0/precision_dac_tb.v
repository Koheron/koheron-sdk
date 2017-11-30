`timescale 1 ns / 1 ps

module precision_dac_tb();

  reg                       clk;
  reg [16*4-1:0]            data;
  reg                       valid;
  reg [4-1:0]               cmd;
  wire                      sclk;
  wire                      sync;
  wire                      sdi;
  wire                      ldac;

  precision_dac DUT (
    .clk(clk),
    .data(data),
    .valid(valid),
    .cmd(cmd),
    .sync(sync),
    .sdi(sdi),
    .ldac(ldac),
    .sclk(sclk)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    valid = 0;
    cmd = 4'b0001;
    data = {{16'b1110100000000001}, {3{16'b1110110000000001}}};
    #(100*CLK_PERIOD)
    valid = 1;
    #(2000*CLK_PERIOD)
    data = {{3{16'b1110110000000001}}, {16'b1110100000000001}};
    #(100000*CLK_PERIOD)
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule
