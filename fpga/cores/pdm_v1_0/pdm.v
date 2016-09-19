// Pulse density Modulator

`timescale 1 ns / 1 ps

module pdm #
(
  parameter NBITS = 11
)
(
  input wire                      clk,
  input wire [NBITS-1:0]          data_in,
  input wire                      rst,

  output reg                      pdm_out,
  output reg [NBITS-1:0]          pdm_error
);
 
  localparam integer MAX = 2**NBITS - 1; 
  
  always @(posedge clk) begin
    if (rst == 1'b1) begin
      pdm_out <= 0;
      pdm_error <= 0;
    end  
    else if (data_in >= pdm_error) begin
      pdm_out <= 1;
      pdm_error <= pdm_error + MAX - data_in;
    end else begin
      pdm_out <= 0;
      pdm_error <= pdm_error - data_in;
    end
  end

endmodule
