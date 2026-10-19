`timescale 1 ns / 1 ps

// Capture a coherent input epoch from a slower, phase-related sample clock.
// The arithmetic downstream runs continuously; valid marks distinct samples.
module sampled_mixer_input (
    input wire clk, resetn, valid_in,
    input wire [31:0] a_in, b_in,
    input wire [7:0] rounding_in,
    output reg [31:0] a_out=0, b_out=0,
    output reg [7:0] rounding_out=0,
    output reg valid_out=0
);
    always @(posedge clk) begin
        if (!resetn) begin
            valid_out <= 0;
            a_out <= 0;
            b_out <= 0;
            rounding_out <= 0;
        end else begin
            valid_out <= valid_in;
            if (valid_in) begin
                a_out <= a_in;
                b_out <= b_in;
                rounding_out <= rounding_in;
            end
        end
    end
endmodule
