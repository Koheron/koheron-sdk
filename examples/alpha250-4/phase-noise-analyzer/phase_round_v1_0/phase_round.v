`timescale 1 ns / 1 ps

// Convert 24-bit-CORDIC phase counts to the existing 16-bit-CORDIC count
// scale before the CIC. E[rounded_phase] = phase / 256 away from the modulo
// counter boundary. The low 32 bits preserve the former counter range.
module phase_round (
  input wire clk,
  input wire aresetn,
  input wire signed [39:0] phase,
  input wire [7:0] random_round,
  output reg signed [31:0] rounded_phase
);
  wire signed [40:0] sum = $signed({phase[39], phase})
                         + $signed({33'b0, random_round});
  always @(posedge clk)
    if (!aresetn) rounded_phase <= 0;
    else rounded_phase <= sum >>> 8;
endmodule
