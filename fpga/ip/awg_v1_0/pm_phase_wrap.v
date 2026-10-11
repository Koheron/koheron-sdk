`timescale 1ns / 1ps

// Compute the phase accumulator's carry without traversing the full phase
// adder. Independent short sums feed a block carry-lookahead network.
module pm_phase_wrap #(
    parameter integer WIDTH = 48
) (
    input wire [WIDTH-1:0] a, b,
    output wire carry
);
    localparam integer BLOCK_BITS = 8;
    localparam integer BLOCKS = (WIDTH + BLOCK_BITS - 1) / BLOCK_BITS;
    wire [BLOCKS-1:0] generate_carry, propagate_carry, terms;

    genvar block;
    generate for (block = 0; block < BLOCKS; block = block+1) begin : g_block
        localparam integer START = block * BLOCK_BITS;
        localparam integer BITS = WIDTH - START < BLOCK_BITS ? WIDTH - START : BLOCK_BITS;
        wire [BITS:0] partial_sum = {1'b0, a[START +: BITS]} + {1'b0, b[START +: BITS]};
        assign generate_carry[block] = partial_sum[BITS];
        assign propagate_carry[block] = &(a[START +: BITS] ^ b[START +: BITS]);
        if (block == BLOCKS-1) begin : g_last
            assign terms[block] = generate_carry[block];
        end else begin : g_propagated
            assign terms[block] = generate_carry[block] &
                (&propagate_carry[BLOCKS-1:block+1]);
        end
    end endgenerate
    assign carry = |terms;
endmodule
