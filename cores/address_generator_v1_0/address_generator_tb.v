`timescale 1 ns / 1 ps

module address_generator_tb();
  
  parameter COUNT_WIDTH = 5;

  reg                     sclr;
  reg [COUNT_WIDTH-1:0]   count_max;
  reg                     clk;
  wire [COUNT_WIDTH+1:0]   address;

  address_generator #(.COUNT_WIDTH(COUNT_WIDTH)) 
  DUT (
    .sclr(sclr),
    .count_max(count_max),
    .clk(clk),
    .address(address)  
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    count_max = 63;
    #(10*CLK_PERIOD) sclr = 1;
    #(1*CLK_PERIOD) sclr = 0;
    #(100*CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


