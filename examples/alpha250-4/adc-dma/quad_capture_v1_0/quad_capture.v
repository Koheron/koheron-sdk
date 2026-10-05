// Two channels per stream, two consecutive sample times per 64-bit beat.
// Each ADC code keeps its 16-bit representation. ADCs cannot be stalled.
module quad_capture #(
    parameter integer PACKET_BEATS = 32768,
    parameter integer MAX_SAMPLES = 16777216
) (
    input wire clk,
    input wire aresetn,
    input wire trigger,
    input wire [31:0] sample_count,
    input wire test_pattern,
    input wire [15:0] adc00, adc01, adc10, adc11,
    output wire [31:0] status,
    output wire [31:0] captured_samples,
    output reg [63:0] m0_axis_tdata,
    output reg m0_axis_tvalid,
    input wire m0_axis_tready,
    output reg m0_axis_tlast,
    output wire [7:0] m0_axis_tkeep,
    output reg [63:0] m1_axis_tdata,
    output reg m1_axis_tvalid,
    input wire m1_axis_tready,
    output reg m1_axis_tlast,
    output wire [7:0] m1_axis_tkeep
);
    reg active, done, overflow0, overflow1, invalid, draining;
    reg trigger_d, pattern;
    reg [24:0] count, last_sample;
    reg [24:0] overflow_at;
    reg [14:0] beat;
    reg [31:0] first0, first1;
    wire [15:0] index_code = count[15:0];
    wire [15:0] index_high = {7'b0, count[24:16]};
    // Each pair encodes the full index, so whole repeated/reordered DMA packets
    // are detectable even though the low 16-bit counter wraps every packet.
    wire [31:0] frame0 = pattern ? {index_high ^ 16'h4000, index_code} : {adc01, adc00};
    wire [31:0] frame1 = pattern ? {index_high ^ 16'hc000, index_code ^ 16'h8000} : {adc11, adc10};
    wire stalled0 = m0_axis_tvalid && !m0_axis_tready;
    wire stalled1 = m1_axis_tvalid && !m1_axis_tready;
    assign status = {26'b0, draining, invalid, overflow1, overflow0, done, active};
    assign captured_samples = {7'b0, (overflow0 || overflow1) ? overflow_at : count};
    assign m0_axis_tkeep = 8'hff;
    assign m1_axis_tkeep = 8'hff;

    always @(posedge clk) begin
        if (!aresetn) begin
            active <= 0; done <= 0; overflow0 <= 0; overflow1 <= 0; invalid <= 0;
            draining <= 0; overflow_at <= 0;
            trigger_d <= 0; pattern <= 0; count <= 0; last_sample <= 0; beat <= 0;
            first0 <= 0; first1 <= 0;
            m0_axis_tdata <= 0; m0_axis_tvalid <= 0; m0_axis_tlast <= 0;
            m1_axis_tdata <= 0; m1_axis_tvalid <= 0; m1_axis_tlast <= 0;
        end else begin
            trigger_d <= trigger;
            if (m0_axis_tready) m0_axis_tvalid <= 0;
            if (m1_axis_tready) m1_axis_tvalid <= 0;
            if (trigger && !trigger_d && !active && !done && !overflow0 && !overflow1 && !invalid) begin
                if (sample_count < 2 || sample_count > MAX_SAMPLES || sample_count[0]) begin
                    invalid <= 1;
                end else begin
                    active <= 1;
                    pattern <= test_pattern;
                    last_sample <= sample_count[24:0] - 1'b1;
                    // The shared trigger captures sample zero on this edge.
                    first0 <= test_pattern ? 32'h40000000 : {adc01, adc00};
                    first1 <= test_pattern ? 32'hc0008000 : {adc11, adc10};
                    count <= 1;
                end
            end else if (draining) begin
                // An invalid record still has to finish both DMA packets/chains.
                // Wait for pending beats, then pad the remaining slots with zeros.
                if (!stalled0 && !stalled1) begin
                    m0_axis_tdata <= 0; m1_axis_tdata <= 0;
                    m0_axis_tvalid <= 1; m1_axis_tvalid <= 1;
                    m0_axis_tlast <= (beat == PACKET_BEATS-1 || count[24:1] == last_sample[24:1]);
                    m1_axis_tlast <= (beat == PACKET_BEATS-1 || count[24:1] == last_sample[24:1]);
                    beat <= (beat == PACKET_BEATS-1) ? 0 : beat + 1'b1;
                    count <= count + 2'd2;
                    if (count[24:1] == last_sample[24:1]) begin
                        draining <= 0;
                        done <= 1;
                    end
                end
            end else if (active) begin
                count <= count + 1'b1;
                if (!count[0]) begin
                    first0 <= frame0;
                    first1 <= frame1;
                end else if (stalled0 || stalled1) begin
                    // Preserve pending AXIS data and reject the record. Finish
                    // outstanding DMA work instead of cutting off a packet.
                    overflow0 <= stalled0;
                    overflow1 <= stalled1;
                    active <= 0;
                    draining <= 1;
                    overflow_at <= count - 1'b1;
                    count <= count - 1'b1;
                end else begin
                    m0_axis_tdata <= {frame0, first0};
                    m1_axis_tdata <= {frame1, first1};
                    m0_axis_tvalid <= 1;
                    m1_axis_tvalid <= 1;
                    m0_axis_tlast <= (beat == PACKET_BEATS-1 || count == last_sample);
                    m1_axis_tlast <= (beat == PACKET_BEATS-1 || count == last_sample);
                    beat <= (beat == PACKET_BEATS-1) ? 0 : beat + 1'b1;
                    if (count == last_sample) begin
                        active <= 0;
                        done <= 1;
                    end
                end
            end
        end
    end
endmodule
