`timescale 1 ns / 1 ps

module peak_detector_tb();

  parameter WFM_WIDTH = 8;
  
  reg clk;
  reg [31:0] din;
  reg [WFM_WIDTH-1:0] address_low;
  reg [WFM_WIDTH-1:0] address_high;
  reg [WFM_WIDTH-1:0] address_reset;
  reg s_axis_tvalid;

  wire [WFM_WIDTH-1:0] address_out;
  wire [31:0] maximum_out;
  wire m_axis_tvalid;

  system_wrapper
  DUT (
    .clk(clk),
    .din(din),
    .address_low(address_low),
    .address_high(address_high),
    .address_reset(address_reset),
    .s_axis_tvalid(s_axis_tvalid),
    .address_out(address_out),
    .maximum_out(maximum_out),
    .m_axis_tvalid(m_axis_tvalid)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 0;
    din = 0;
    address_low = 0;
    address_high = 2**WFM_WIDTH-1;
    address_reset = (address_low+2**WFM_WIDTH - 1) % (2**WFM_WIDTH);
    s_axis_tvalid = 0;
    #(2**WFM_WIDTH * CLK_PERIOD) s_axis_tvalid = 1;
    #(10000 * CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

  always begin
    #(CLK_PERIOD * 2**(WFM_WIDTH-2)) din = 1;
    #(CLK_PERIOD * 2**(WFM_WIDTH-2)) din = 2;
    #(CLK_PERIOD * 2**(WFM_WIDTH-2)) din = 3;
    #(CLK_PERIOD * 2**(WFM_WIDTH-2)) din = 0;
  end
  
endmodule


