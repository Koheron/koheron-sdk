`timescale 1 ns / 1 ps

module at93c46d_spi_tb();

  reg                       clk;
  reg [8-1:0]               cmd;
  reg [16-1:0]              data_in;
  reg                       start;
  reg                       dout;
  wire                      cs;
  wire                      sclk;
  wire                      din;
  wire [8-1:0]              cnt_sclk_out;
  wire [16-1:0]             data_out;

  at93c46d_spi DUT (
    .clk(clk),
    .cmd(cmd),
    .data_in(data_in),
    .start(start),
    .dout(dout),
    .cs(cs),
    .sclk(sclk),
    .din(din),
    .cnt_sclk_out(cnt_sclk_out),
    .data_out(data_out)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    start = 0;
    cmd = 8'b00110101; // Write instruction
    data_in = 16'b1110100000000001;
    dout = 1;
    #(1000*CLK_PERIOD)
    start = 1;
    #(2000*CLK_PERIOD)
    start = 0;
    cmd = 8'b10100011; // Read instruction
    #(4000*CLK_PERIOD)
    start = 1;
    #(100000*CLK_PERIOD)
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule
