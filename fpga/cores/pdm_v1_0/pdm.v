// Pulse density Modulator

`timescale 1 ns / 1 ps

module pdm #
(
  parameter NBITS = 11
)
(
  input wire                      clk,
  input wire [NBITS-1:0]          din,
  input wire                      rst,

  output reg                      dout,
  output reg [NBITS-1:0]          error
);
 
  localparam integer MAX = 2**NBITS - 1;
  reg rst_reg;
  reg [NBITS-1:0] din_reg;
  reg [NBITS-1:0] error_0;
  reg [NBITS-1:0] error_1;
  
  always @(posedge clk) begin
    rst_reg <= rst;
    din_reg <= din;
    error_1 <= error + MAX - din_reg;
    error_0 <= error - din_reg;
  end

  always @(posedge clk) begin
    if (rst_reg == 1'b1) begin
      dout <= 0;
    end else begin
      dout <= (din_reg >= error);
    end
  end

  always @(posedge clk) begin
    if (rst_reg == 1'b1) begin
      error <= 0;
    end else if (dout) begin
      error <= error_1;
    end else begin
      error <= error_0;
    end
  end


endmodule
