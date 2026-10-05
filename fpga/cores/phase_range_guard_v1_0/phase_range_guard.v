`timescale 1 ns / 1 ps

// Keep common carrier phase wide until after the pair subtraction. Only the
// differential phase is constrained to the CIC input range. Saturate rather
// than wrapping and report range loss to the packet's sticky overflow guard.
module phase_range_guard #(
    parameter integer INPUT_WIDTH = 65,
    parameter integer OUTPUT_WIDTH = 32
) (
    input wire clk,
    input wire aresetn,
    input wire signed [INPUT_WIDTH-1:0] din,
    output reg signed [OUTPUT_WIDTH-1:0] dout,
    output reg overflow
);
    wire outside = din[INPUT_WIDTH-1:OUTPUT_WIDTH-1] !=
        {(INPUT_WIDTH-OUTPUT_WIDTH+1){din[OUTPUT_WIDTH-1]}};
    always @(posedge clk) begin
        if (!aresetn) begin
            dout <= 0;
            overflow <= 0;
        end else begin
            overflow <= outside;
            dout <= outside ? (din[INPUT_WIDTH-1] ?
                {1'b1, {(OUTPUT_WIDTH-1){1'b0}}} :
                {1'b0, {(OUTPUT_WIDTH-1){1'b1}}}) : din[OUTPUT_WIDTH-1:0];
        end
    end
endmodule
