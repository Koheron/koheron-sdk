// Register input when ce pulses high

`timescale 1 ns / 1 ps

module register #
(
  parameter NBITS = 16
)
(
  input wire                      clk,
  input wire                      ce,
  input wire [NBITS-1:0]          din,

  output reg [NBITS-1:0]          dout
  
);
 

 
  always @(posedge clk) begin
    if (ce)
    begin
    dout <= din;

    end
  end

endmodule
