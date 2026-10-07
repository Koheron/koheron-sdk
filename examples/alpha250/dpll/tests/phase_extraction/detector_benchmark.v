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
    output reg signed [39:0] phase = 0,
    output reg signed [24:0] frequency = 0,
    output reg error = 0,
    output reg phase_valid = 0
);
    reg signed [23:0] i_reg=0, q_reg=0;
    reg reset_reg=0, valid_reg=0;
    wire signed [23:0] p;
    wire v;
    wire signed [39:0] accumulated;
    wire signed [24:0] difference;
    wire overflow;
    phase_unwrapper #(.DIN_WIDTH(24), .DOUT_WIDTH(40), .FUSED_DIFFERENCE(1), .CANONICAL_INPUT(1)) unwrap(
        .clk(clk), .rst(!reset_reg), .acc_on(valid_reg), .phase_in(p),
        .phase_out(accumulated), .freq_out(difference), .overflow(overflow));
    always @(posedge clk) begin
        i_reg<=i_in; q_reg<=q_in; reset_reg<=resetn; valid_reg<=valid;
        phase<=accumulated; frequency<=difference; error<=overflow; phase_valid<=v;
    end
    phase_extractor #(.ROTATIONS_PER_CLOCK(ROTATIONS_PER_CLOCK), .PAIR_START(PAIR_START),
                      .COMPACT_PREP(COMPACT_PREP), .FUSE_ROUND(FUSE_ROUND), .RESIDUAL_CORRECTION(RESIDUAL_CORRECTION)) extractor(clk,reset_reg,valid_reg,i_reg,q_reg,v,p);
endmodule
