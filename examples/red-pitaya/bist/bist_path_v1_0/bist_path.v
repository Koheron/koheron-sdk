`timescale 1ns/1ps
// All signals are synchronous to the converter clock. A downstream AXIS FIFO
// absorbs DDR stalls. Overflow means lost analog samples and fails the capture.
module bist_path(
    input wire aclk, input wire aresetn, input wire dna_clk,
    input wire [31:0] mode, capture, samples, snapshot, reset_diag, seed,
    input wire [31:0] s_axis_tdata, input wire [3:0] s_axis_tkeep,
    input wire s_axis_tvalid, s_axis_tlast, output wire s_axis_tready,
    output wire [31:0] m_axis_tdata, output wire [3:0] m_axis_tkeep,
    output wire m_axis_tvalid, m_axis_tlast, input wire m_axis_tready,
    input wire [13:0] adc0, adc1, output wire [13:0] dac0, dac1,
    output wire [31:0] bram_addr, bram_data, output wire [3:0] bram_we,
    input wire [31:0] bram_q,
    output reg [63:0] cycles_snapshot, tx_snapshot, rx_snapshot,
    output wire data_resetn,
    output wire [63:0] dna,
    output reg [31:0] overflow_snapshot, packets_snapshot, discontinuities_snapshot, capture_done, snapshot_id
);
    reg [6:0] dna_step = 0;
    reg [56:0] dna_value = 0;
    wire dna_bit;
    assign dna = {7'b0,dna_value};
    DNA_PORT dna_port(.CLK(dna_clk), .DIN(1'b0), .DOUT(dna_bit),
                      .READ(dna_step == 0), .SHIFT(dna_step >= 1 && dna_step <= 57));
    always @(posedge dna_clk) begin
        if (dna_step < 58) dna_step <= dna_step + 1;
        if (dna_step >= 1 && dna_step <= 57) dna_value <= {dna_bit,dna_value[56:1]};
    end
    assign data_resetn = aresetn && !mode[8];
    reg [2:0] sample_divider;
    reg [31:0] capture_data;
    reg capture_valid, capture_last;
    reg [9:0] playback_address, upload_address;
    reg [31:0] previous_capture, previous_snapshot, previous_reset, remaining;
    reg [63:0] cycles, tx_words, rx_words;
    reg [31:0] overflow, packets, discontinuities, expected_word;
    wire loopback = mode[1:0] == 0;
    wire upload = mode[1:0] == 1;
    wire play = mode[1:0] == 2;
    wire tx_fire = s_axis_tvalid && s_axis_tready;
    wire rx_fire = m_axis_tvalid && m_axis_tready;
    assign s_axis_tready = upload || (loopback && m_axis_tready);
    assign m_axis_tvalid = data_resetn && (loopback ? s_axis_tvalid : capture_valid);
    assign m_axis_tdata = loopback ? s_axis_tdata : capture_data;
    assign m_axis_tkeep = loopback ? s_axis_tkeep : 4'hf;
    assign m_axis_tlast = loopback ? s_axis_tlast : capture_last;
    // blk_mem_gen in BRAM-controller mode uses byte addresses on both ports.
    assign bram_addr = {20'b0,(upload ? upload_address : playback_address),2'b0};
    assign bram_data = s_axis_tdata;
    assign bram_we = upload && tx_fire ? s_axis_tkeep : 4'b0;
    assign dac0 = play ? bram_q[13:0] : 14'b0;
    assign dac1 = play ? bram_q[29:16] : 14'b0;
    always @(posedge aclk) begin
        if (!data_resetn) begin
            sample_divider <= 0; capture_valid <= 0; capture_last <= 0; capture_data <= 0;
            playback_address <= 0; upload_address <= 0; remaining <= 0;
            previous_capture <= 0; previous_snapshot <= 0; previous_reset <= 0;
            cycles <= 0; tx_words <= 0; rx_words <= 0; overflow <= 0; packets <= 0; discontinuities <= 0; expected_word <= 0;
            cycles_snapshot <= 0; tx_snapshot <= 0; rx_snapshot <= 0;
            overflow_snapshot <= 0; packets_snapshot <= 0; discontinuities_snapshot <= 0; capture_done <= 0; snapshot_id <= 0;
        end else begin
            cycles <= cycles + 1;
            previous_capture <= capture; previous_snapshot <= snapshot; previous_reset <= reset_diag;
            playback_address <= play ? playback_address + 1 : 0;
            if (upload && tx_fire) upload_address <= upload_address + 1;
            if (!upload) upload_address <= 0;
            sample_divider <= sample_divider + 1;
            if (capture_valid && m_axis_tready) begin
                capture_valid <= 0;
                if (capture_last) capture_done <= 1;
            end
            if (capture != previous_capture && play) begin remaining <= samples; capture_done <= 0; end
            else if (play && remaining != 0 && sample_divider == 0) begin
                if (!capture_valid || m_axis_tready) begin
                    capture_data <= {2'b0,adc1,2'b0,adc0};
                    capture_valid <= 1; capture_last <= remaining == 1;
                    remaining <= remaining - 1;
                end else overflow <= overflow + 1;
            end
            if (!play) begin remaining <= 0; capture_valid <= 0; end
            if (tx_fire) begin
                tx_words <= tx_words + 1;
                if (loopback) begin
                    if ((s_axis_tdata ^ seed) != expected_word) discontinuities <= discontinuities + 1;
                    expected_word <= expected_word + 32'h9e3779b9;
                end
            end
            if (rx_fire) begin rx_words <= rx_words + 1; if (m_axis_tlast) packets <= packets + 1; end
            if (reset_diag != previous_reset) begin tx_words <= 0; rx_words <= 0; overflow <= 0; packets <= 0; end
            if (snapshot != previous_snapshot) begin
                cycles_snapshot <= cycles; tx_snapshot <= tx_words; rx_snapshot <= rx_words;
                overflow_snapshot <= overflow; packets_snapshot <= packets; discontinuities_snapshot <= discontinuities; snapshot_id <= snapshot;
            end
        end
    end
endmodule
