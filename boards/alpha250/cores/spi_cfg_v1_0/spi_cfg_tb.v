`timescale 1 ns / 1 ps

module spi_cfg_tb();

  reg                       aclk;
  reg [32-1:0]              s_axis_tdata;
  reg                       s_axis_tvalid;
  reg [8-1:0]               cmd;
  wire                      sclk;
  wire [4-1:0]              cs;
  wire                      sdi;
  wire                      s_axis_tready;

  spi_cfg DUT (
    .aclk(aclk),
    .s_axis_tdata(s_axis_tdata),
    .s_axis_tvalid(s_axis_tvalid),
    .cmd(cmd),
    .cs(cs),
    .sdi(sdi),
    .sclk(sclk),
    .s_axis_tready(s_axis_tready)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    aclk = 1;
    s_axis_tvalid = 0;
    cmd = 8'b000001010;
    s_axis_tdata = 32'b11000000000000011110100000000001;
    #(100*CLK_PERIOD)
    s_axis_tvalid = 1;
    #(10*CLK_PERIOD)
    s_axis_tvalid = 0;
    #(2000*CLK_PERIOD)
    cmd = 8'b000001010;
    s_axis_tdata = 32'b11101000000000110000000000000001;
    #(10*CLK_PERIOD)
    s_axis_tvalid = 1;
    #(100000*CLK_PERIOD)
    $finish;
  end

  always #(CLK_PERIOD/2) aclk = ~aclk;

endmodule
