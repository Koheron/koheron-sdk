`timescale 1ns/1ps
// Slow half of the shared split six-stage CIC. The input is the untruncated
// 38-bit fixed /2 result. Configure with the TOTAL even rate (4..8192), so
// normalization remains identical to the existing 32-to-40-bit PNA CIC.
// Full 110-bit modular arithmetic supports every rate without stage pruning.
module phase_cic_decimator (
    input wire aclk,
    input wire aresetn,
    input wire [15:0] total_rate,
    input wire [39:0] s_axis_tdata,
    input wire s_axis_tvalid,
    output wire s_axis_tready,
    output reg [39:0] m_axis_tdata,
    output reg m_axis_tvalid,
    input wire m_axis_tready
);
    localparam WIDTH = 110;
    reg configured;
    reg [12:0] rate, count;
    reg [6:0] shift;
    reg [6:0] shift_rom [0:4095];
    // ceil(log2(R^6))-8: the same power-of-two normalization used by PG140
    // and phase_calibration::cic_correction. Evaluated only at elaboration.
    function [6:0] rate_shift(input integer r);
        reg [79:0] gain;
        integer i, bits;
        begin
            gain=1;
            for (i=0; i<6; i=i+1) gain=gain*r;
            gain=gain-1;
            bits=0;
            for (i=0; i<80; i=i+1) if (gain[i]) bits=i+1;
            rate_shift=bits-8;
        end
    endfunction
    integer entry;
    initial for (entry=0; entry<4096; entry=entry+1)
        shift_rom[entry]=rate_shift(2*(entry+1));
    wire [12:0] requested_rate=total_rate[13:1];
    wire [12:0] shift_index=requested_rate-13'd1;
    wire advance = !m_axis_tvalid || m_axis_tready;
    assign s_axis_tready = configured && advance;
    wire accepted = s_axis_tvalid && s_axis_tready;
    wire decimate = accepted && count==rate-1'b1;
    reg signed [WIDTH-1:0] integrator [0:5];
    reg signed [WIDTH-1:0] comb [0:5];
    reg signed [WIDTH-1:0] history [0:5];
    reg [5:0] comb_valid;
    reg signed [WIDTH-1:0] scaled;
    reg scaled_valid;
    wire signed [WIDTH-1:0] incoming = {{72{s_axis_tdata[37]}},s_axis_tdata[37:0]};
    wire signed [WIDTH-1:0] last_sum = integrator[5]+integrator[4];
    integer stage;
    always @(posedge aclk) begin
        if (!aresetn) begin
            configured <= 0;
            rate <= 0;
            count <= 0;
            shift <= 0;
            comb_valid <= 0;
            scaled_valid <= 0;
            scaled <= 0;
            m_axis_tvalid <= 0;
            m_axis_tdata <= 0;
            for (stage=0; stage<6; stage=stage+1) begin
                integrator[stage] <= 0;
                comb[stage] <= 0;
                history[stage] <= 0;
            end
        end else begin
            // total_rate has settled while reset is asserted. Rate changes
            // require a fresh epoch; it never controls live fast-clock logic.
            if (!configured && total_rate>=4 && total_rate<=8192 && !total_rate[0]) begin
                rate <= requested_rate;
                shift <= shift_rom[shift_index[11:0]];
                configured <= 1;
            end
            if (advance) begin
                comb_valid[0] <= decimate;
                for (stage=1; stage<6; stage=stage+1)
                    comb_valid[stage] <= comb_valid[stage-1];
                if (accepted) begin
                    count <= decimate ? 13'd0 : count+1'b1;
                    integrator[0] <= integrator[0]+incoming;
                    for (stage=1; stage<6; stage=stage+1)
                        integrator[stage] <= integrator[stage]+integrator[stage-1];
                end
                if (decimate) begin
                    comb[0] <= last_sum-history[0];
                    history[0] <= last_sum;
                end
                for (stage=1; stage<6; stage=stage+1) begin
                    if (comb_valid[stage-1]) begin
                        comb[stage] <= comb[stage-1]-history[stage];
                        history[stage] <= comb[stage-1];
                    end
                end
                // Two registered shifter stages keep rate normalization out
                // of the integrator/comb carry paths.
                scaled_valid <= comb_valid[5];
                if (comb_valid[5]) scaled <= comb[5] >>> shift[2:0];
                m_axis_tvalid <= scaled_valid;
                if (scaled_valid) m_axis_tdata <= scaled >>> {shift[6:3],3'b000};
            end
        end
    end
endmodule
