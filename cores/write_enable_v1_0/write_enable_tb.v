`timescale 1 ns / 1 ps

module write_enable_tb();
  
  parameter BRAM_WIDTH = 5;

  reg                   restart;
  reg  [BRAM_WIDTH-1:0] address;
  reg                   clk;
  wire                  wen;
  wire [BRAM_WIDTH-1:0] count;
  wire                  init;

  write_enable #(.BRAM_WIDTH(BRAM_WIDTH)) DUT (
    .restart(restart),
    .address(address),
    .clk(clk),
    .wen(wen),
    .count(count),
    .init(init)    
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    restart = 0;
    address = 5'b00110;
    #(10*CLK_PERIOD) restart = 1;
    #(1*CLK_PERIOD) restart = 0;
    #(5*CLK_PERIOD) address = 5'b11111;
    #(CLK_PERIOD) address = 5'b00000;
    #(100*CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


