`timescale 1ns / 1ps

module tlast_gen
#(
    parameter TDATA_WIDTH = 8,
    parameter PKT_LENGTH  = 1024*1024
)
(
    input                            aclk,
    input                            resetn,

    // Trigger control
    input                            trig,  // One-shot trigger input (rising edge)

    // Slave interface
    input                            s_axis_tvalid,
    output                           s_axis_tready,
    input  [TDATA_WIDTH-1:0]         s_axis_tdata,

    // Master interface
    output                           m_axis_tvalid,
    input                            m_axis_tready,
    output                           m_axis_tlast,
    output [TDATA_WIDTH-1:0]         m_axis_tdata
);

    // Edge detection for trig
    reg trig_d = 0;
    wire trig_rise = trig & ~trig_d;

    always @(posedge aclk)
        trig_d <= trig;

    // Latch activation after trigger
    reg capture_enabled = 0;
    always @(posedge aclk) begin
        if (~resetn)
            capture_enabled <= 0;
        else if (trig_rise)
            capture_enabled <= 1;
    end

    // Stream gating
    assign s_axis_tready = capture_enabled ? m_axis_tready : 1'b0;
    assign m_axis_tvalid = capture_enabled ? s_axis_tvalid : 1'b0;
    assign m_axis_tdata  = s_axis_tdata;

    // Sample counter
    wire new_sample = s_axis_tvalid & s_axis_tready;
    reg [$clog2(PKT_LENGTH):0] cnt = 0;

    always @(posedge aclk) begin
        if (~resetn || (m_axis_tlast & new_sample))
            cnt <= 0;
        else if (new_sample)
            cnt <= cnt + 1;
    end

    assign m_axis_tlast = capture_enabled && (cnt == PKT_LENGTH-1);

endmodule