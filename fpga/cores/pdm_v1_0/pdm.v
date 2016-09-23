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
  reg [NBITS-1:0] data_in_reg;
  reg [NBITS-1:0] pdm_error_0;
  reg [NBITS-1:0] pdm_error_1;
  
  always @(posedge clk) begin
    data_in_reg <= data_in;
    pdm_error_1 <= pdm_error + MAX - data_in_reg;
    pdm_error_0 <= pdm_error - data_in_reg;
  end
  
  always @(posedge clk) begin
    if (rst == 1'b1) begin
      pdm_out <= 0;
      pdm_error <= 0;
    end  
    else if (data_in_reg >= pdm_error) begin
      pdm_out <= 1;
      pdm_error <= pdm_error_1;
    end else begin
      pdm_out <= 0;
      pdm_error <= pdm_error_0;
    end
  end

endmodule
