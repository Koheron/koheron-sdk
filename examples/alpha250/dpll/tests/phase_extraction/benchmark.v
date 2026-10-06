`timescale 1 ns / 1 ps
module phase_extraction_benchmark #(
    parameter integer ROTATIONS_PER_CLOCK=2,
    parameter integer PAIR_START=8,
    parameter integer COMPACT_PREP=0,
    parameter integer FUSE_ROUND=1,
    parameter integer RESIDUAL_CORRECTION=1
)(
    input wire clk,
    input wire resetn,
    input wire valid,
    input wire signed [23:0] i_in, q_in,
    output reg signed [23:0] phase = 0,
    output reg phase_valid = 0
);
    reg signed [23:0] i_reg=0, q_reg=0;
    reg reset_reg=0, valid_reg=0;
    wire signed [23:0] p;
    wire v;
    always @(posedge clk) begin
        i_reg<=i_in; q_reg<=q_in; reset_reg<=resetn; valid_reg<=valid;
        phase<=p; phase_valid<=v;
    end
    phase_extractor #(.ROTATIONS_PER_CLOCK(ROTATIONS_PER_CLOCK), .PAIR_START(PAIR_START),
                      .COMPACT_PREP(COMPACT_PREP), .FUSE_ROUND(FUSE_ROUND), .RESIDUAL_CORRECTION(RESIDUAL_CORRECTION)) extractor(clk,reset_reg,valid_reg,i_reg,q_reg,v,p);
endmodule
