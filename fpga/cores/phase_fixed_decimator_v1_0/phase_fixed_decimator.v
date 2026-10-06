`timescale 1ns/1ps
// Six full-precision (1+z^-1) sums, then /2. This is a six-stage CIC /2
// without wide, high-clock integrators. The five interstage pipeline samples
// add a fixed delay; the response and DC gain (64) are unchanged.
module phase_fixed_decimator (
    input wire aclk,
    input wire aresetn,
    input wire [31:0] s_axis_tdata,
    input wire s_axis_tvalid,
    output wire s_axis_tready,
    output reg [39:0] m_axis_tdata,
    output reg m_axis_tvalid,
    input wire m_axis_tready
);
    wire advance = !m_axis_tvalid || m_axis_tready;
    // PRIME must wait for the downstream FIFO to leave reset. An empty local
    // output register alone does not mean the live ADC stream can be admitted.
    assign s_axis_tready = m_axis_tready;
    wire accepted = s_axis_tvalid && s_axis_tready;
    reg odd;
    wire signed [37:0] sums [0:6];
    assign sums[0] = {{6{s_axis_tdata[31]}}, s_axis_tdata};
    genvar stage;
    generate for (stage=0; stage<6; stage=stage+1) begin : stages
        localparam WIDTH = 32+stage;
        reg signed [WIDTH-1:0] previous;
        wire signed [WIDTH:0] value = $signed(sums[stage][WIDTH-1:0]) +
                                     $signed({previous[WIDTH-1], previous});
        if (stage<5) begin : pipelined
            reg signed [WIDTH:0] sum;
            assign sums[stage+1] = sum;
            always @(posedge aclk) begin
                if (!aresetn) sum <= 0;
                else if (accepted) sum <= value;
            end
        end else assign sums[stage+1] = value;
        always @(posedge aclk) begin
            if (!aresetn) previous <= 0;
            else if (accepted) previous <= sums[stage][WIDTH-1:0];
        end
    end endgenerate
    always @(posedge aclk) begin
        if (!aresetn) begin
            odd <= 0;
            m_axis_tvalid <= 0;
            m_axis_tdata <= 0;
        end else if (advance) begin
            m_axis_tvalid <= accepted && odd;
            if (accepted) begin
                odd <= !odd;
                if (odd) m_axis_tdata <= {{2{sums[6][37]}}, sums[6]};
            end
        end
    end
endmodule
