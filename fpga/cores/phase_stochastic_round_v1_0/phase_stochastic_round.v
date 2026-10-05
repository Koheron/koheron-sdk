`timescale 1 ns / 1 ps

// CORDIC calculates 24-bit scaled radians. Return to the existing 16-bit
// phase unit with unbiased rounding, rather than a deterministic staircase.
// CIC averaging can retain sub-LSB changes without coherent LO harmonics.
module phase_stochastic_round (
    input wire clk,
    input wire aresetn,
    input wire signed [23:0] phase_in,
    input wire [7:0] random_round,
    output reg signed [15:0] phase_out
);
    wire signed [24:0] rounded = $signed({phase_in[23], phase_in}) +
        $signed({17'b0, random_round});
    always @(posedge clk) begin
        if (!aresetn) phase_out <= 0;
        else phase_out <= rounded >>> 8;
    end
endmodule
