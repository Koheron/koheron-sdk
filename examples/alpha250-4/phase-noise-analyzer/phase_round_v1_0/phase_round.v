`timescale 1 ns / 1 ps

// Stochastic signed fixed-point conversion: E[output] = input / 2^SHIFT.
// Random words must be uniform and independent of the input. Output narrowing
// is modulo OUTPUT_WIDTH, preserving the phase-counter wrap when used there.
module phase_round #(
  parameter integer INPUT_WIDTH = 40,
  parameter integer SHIFT = 8,
  parameter integer OUTPUT_WIDTH = 32
) (
  input wire clk,
  input wire aresetn,
  input wire signed [INPUT_WIDTH-1:0] phase,
  input wire [SHIFT-1:0] random_round,
  output reg signed [OUTPUT_WIDTH-1:0] rounded_phase
);
  wire signed [INPUT_WIDTH:0] sum = $signed({phase[INPUT_WIDTH-1], phase})
                         + $signed({{(INPUT_WIDTH+1-SHIFT){1'b0}}, random_round});
  always @(posedge clk)
    if (!aresetn) rounded_phase <= 0;
    else rounded_phase <= sum >>> SHIFT;
endmodule
