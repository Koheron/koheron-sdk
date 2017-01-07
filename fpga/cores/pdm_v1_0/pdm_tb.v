`timescale 1 ns / 1 ps

module pdm_tb();
  parameter NBITS = 10;
 
  reg                       clk;
  reg                       rst;
  reg [NBITS-1:0]           din;
  wire                      dout;
  wire [NBITS-1:0]          error;

  pdm # (
    .NBITS(NBITS)
  )
  DUT (
    .clk(clk),
    .rst(rst),
    .din(din),
    .dout(dout),
    .error(error)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    rst = 1;
    din = 120;
    #(10*CLK_PERIOD)
    rst = 0;
    #(1000*CLK_PERIOD)
    din = 500;
    #(1000*CLK_PERIOD)
    din = 900;    
    #(100000*CLK_PERIOD)
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule
