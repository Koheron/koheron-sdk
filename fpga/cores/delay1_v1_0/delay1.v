// Data Delay 1 clock sycle

`timescale 1 ns / 1 ps

module delay1 #
(
  parameter NBITS = 16
)
(
  input wire                      clk,
  input wire [NBITS-1:0]          din,

  output reg [NBITS-1:0]          dout
  
);
 

  reg [NBITS-1:0] din_reg;
 
  always @(posedge clk) begin
    dout <= din_reg;
    din_reg <= din;
 
  end

endmodule
