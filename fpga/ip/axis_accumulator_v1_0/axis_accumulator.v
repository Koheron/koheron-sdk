`timescale 1ns / 1ps

// Float32 frame sums. The arithmetic IP is embedded in the catalog package.
module axis_accumulator #(
    parameter integer FRAME_LENGTH = 8192,
    parameter integer N_FRAMES = 1023,
    parameter integer CHECK_TLAST = 1,
    parameter integer SYNC_ON_RESET = 0
) (
    input wire aclk,
    input wire aresetn,
    input wire [31:0] s_axis_tdata,
    input wire s_axis_tvalid,
    output wire s_axis_tready,
    input wire s_axis_tlast,
    output reg [31:0] m_axis_tdata,
    output reg m_axis_tvalid,
    input wire m_axis_tready,
    output reg m_axis_tlast,
    output reg [31:0] m_axis_tuser,
    output wire [31:0] frame_index,
    output wire [31:0] cycle_index,
    output wire [31:0] result_count,
    output wire frame_error
);
    wire [31:0] add_a, add_b, add_result;
    wire add_valid, result_valid;
    wire [31:0] raw_data, raw_user;
    wire raw_valid, raw_last;
    wire raw_ready = aresetn && (!m_axis_tvalid || m_axis_tready);
    wire result_done = aresetn && m_axis_tvalid && m_axis_tready && m_axis_tlast;
    axis_accumulator_control #(.FRAME_LENGTH(FRAME_LENGTH), .N_FRAMES(N_FRAMES),
        .CHECK_TLAST(CHECK_TLAST), .SYNC_ON_RESET(SYNC_ON_RESET)) control (
        .aclk(aclk), .aresetn(aresetn),
        .s_axis_tdata(s_axis_tdata), .s_axis_tvalid(s_axis_tvalid),
        .s_axis_tready(s_axis_tready), .s_axis_tlast(s_axis_tlast),
        .m_axis_tdata(raw_data), .m_axis_tvalid(raw_valid),
        .m_axis_tready(raw_ready), .m_axis_tlast(raw_last),
        .m_axis_tuser(raw_user), .frame_index(frame_index), .frame_error(frame_error),
        .cycle_index(cycle_index), .result_count(result_count),
        .result_done(result_done),
        .add_a(add_a), .add_b(add_b), .add_valid(add_valid),
        .add_result(add_result), .result_valid(result_valid)
    );
    // Elastic output register isolates BRAM/mux delay from downstream logic.
    // A bank can be reused once its final beat is copied here, but progress
    // and completion counts advance only when the receiver accepts that beat.
    always @(posedge aclk) begin
        if (!aresetn) begin
            m_axis_tvalid <= 0; m_axis_tdata <= 0;
            m_axis_tlast <= 0; m_axis_tuser <= 0;
        end else if (raw_ready) begin
            m_axis_tvalid <= raw_valid;
            if (raw_valid) begin
                m_axis_tdata <= raw_data;
                m_axis_tlast <= raw_last;
                m_axis_tuser <= raw_user;
            end
        end
    end
    axis_accumulator_add adder (
        .aclk(aclk), .aresetn(aresetn),
        .s_axis_a_tdata(add_a), .s_axis_a_tvalid(add_valid),
        .s_axis_b_tdata(add_b), .s_axis_b_tvalid(add_valid),
        .m_axis_result_tdata(add_result), .m_axis_result_tvalid(result_valid)
    );
endmodule
