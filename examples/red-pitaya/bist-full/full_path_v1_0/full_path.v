`timescale 1ns/1ps
// One elastic output beat: VALID/data/KEEP/LAST remain stable under backpressure.
module full_path(
    input wire aclk, aresetn, dna_clk,
    input wire [31:0] mode, capture, samples, snapshot, reset_diag, seed,
    input wire [31:0] stalls, fault, packet_bytes, decimation, irq_period, irq_ack,
    input wire [31:0] s_axis_tdata, input wire [3:0] s_axis_tkeep,
    input wire s_axis_tvalid, s_axis_tlast, output wire s_axis_tready,
    output wire [31:0] m_axis_tdata, output wire [3:0] m_axis_tkeep,
    output wire m_axis_tvalid, m_axis_tlast, input wire m_axis_tready,
    input wire [13:0] adc0, adc1, output wire [13:0] dac0, dac1,
    output wire [31:0] bram_addr, bram_data, output wire [3:0] bram_we,
    input wire [31:0] bram_q,
    output reg [63:0] cycles_snapshot, tx_snapshot, rx_snapshot,
    output wire data_resetn, output wire [63:0] dna,
    output reg [31:0] overflow_snapshot, packets_snapshot, discontinuities_snapshot,
    output reg [31:0] capture_done, snapshot_id, checker_errors_snapshot, keep_errors_snapshot,
    output reg [31:0] input_stalls_snapshot, output_stalls_snapshot, irq_count_snapshot,
    output wire [31:0] irq_pending, output wire irq
);
    reg [6:0] dna_step = 0;
    reg [56:0] dna_value = 0;
    wire dna_bit;
    assign dna = {7'b0,dna_value};
    DNA_PORT dna_port(.CLK(dna_clk), .DIN(1'b0), .DOUT(dna_bit),
        .READ(dna_step == 0), .SHIFT(dna_step >= 1 && dna_step <= 57));
    always @(posedge dna_clk) begin
        if(dna_step < 58) dna_step <= dna_step + 1;
        if(dna_step >= 1 && dna_step <= 57) dna_value <= {dna_bit,dna_value[56:1]};
    end

    assign data_resetn = aresetn && !mode[8];
    wire loopback = mode[2:0] == 0;
    wire upload = mode[2:0] == 1;
    wire play = mode[2:0] == 2;
    wire generate_packet = mode[2:0] == 3;
    wire sink = mode[2:0] == 4;
    reg valid, last;
    reg [31:0] data;
    reg [3:0] keep;
    reg [63:0] cycles, tx_words, rx_words;
    reg [31:0] previous_capture, previous_snapshot, previous_reset, previous_ack;
    reg [31:0] remaining, divider, upload_address, playback_address;
    reg [31:0] source_word, source_index, expected_word, input_index, packet_seen;
    reg [31:0] overflow, packets, discontinuities, checker_errors, keep_errors;
    reg [31:0] input_stalls, output_stalls, timer, irq_count;
    reg pending;
    wire gate_in = cycles[7:0] >= stalls[7:0];
    wire gate_out = cycles[7:0] >= stalls[15:8];
    wire room = !valid || m_axis_tready;
    wire tx_fire = s_axis_tvalid && s_axis_tready;
    wire rx_fire = m_axis_tvalid && m_axis_tready;
    wire [31:0] valid_mask = {{8{s_axis_tkeep[3]}},{8{s_axis_tkeep[2]}},
                               {8{s_axis_tkeep[1]}},{8{s_axis_tkeep[0]}}};
    wire [2:0] input_bytes = {2'b0,s_axis_tkeep[0]} + {2'b0,s_axis_tkeep[1]} +
                             {2'b0,s_axis_tkeep[2]} + {2'b0,s_axis_tkeep[3]};
    assign s_axis_tready = data_resetn && gate_in &&
        (upload || sink || (loopback && room && gate_out));
    assign m_axis_tvalid = data_resetn && valid;
    assign m_axis_tdata = data;
    assign m_axis_tkeep = keep;
    assign m_axis_tlast = last;
    assign bram_addr = upload ? (upload_address << 2) : (playback_address << 2);
    assign bram_data = s_axis_tdata;
    assign bram_we = upload && tx_fire ? s_axis_tkeep : 4'b0;
    assign dac0 = play ? bram_q[13:0] : 14'b0;
    assign dac1 = play ? bram_q[29:16] : 14'b0;
    assign irq_pending = {31'b0,pending};
    assign irq = pending && data_resetn;

    always @(posedge aclk) begin
        if(!data_resetn) begin
            valid <= 0; last <= 0; data <= 0; keep <= 0;
            cycles <= 0; tx_words <= 0; rx_words <= 0;
            previous_capture <= capture; previous_snapshot <= snapshot;
            previous_reset <= reset_diag; previous_ack <= irq_ack;
            remaining <= 0; divider <= 0; upload_address <= 0; playback_address <= 0;
            source_word <= 0; source_index <= 0; expected_word <= 0; input_index <= 0; packet_seen <= 0;
            overflow <= 0; packets <= 0; discontinuities <= 0; checker_errors <= 0; keep_errors <= 0;
            input_stalls <= 0; output_stalls <= 0; timer <= 0; irq_count <= 0; pending <= 0;
            cycles_snapshot <= 0; tx_snapshot <= 0; rx_snapshot <= 0;
            overflow_snapshot <= 0; packets_snapshot <= 0; discontinuities_snapshot <= 0;
            checker_errors_snapshot <= 0; keep_errors_snapshot <= 0;
            input_stalls_snapshot <= 0; output_stalls_snapshot <= 0; irq_count_snapshot <= 0;
            capture_done <= 0; snapshot_id <= 0;
        end else begin
            cycles <= cycles + 1;
            previous_capture <= capture; previous_snapshot <= snapshot;
            previous_reset <= reset_diag; previous_ack <= irq_ack;
            playback_address <= play ? ((playback_address + 1) & 1023) : 0;
            if(!upload) upload_address <= 0;
            if(s_axis_tvalid && !s_axis_tready) input_stalls <= input_stalls + 1;
            if(valid && !m_axis_tready) output_stalls <= output_stalls + 1;
            if(rx_fire) begin
                valid <= 0; rx_words <= rx_words + 1;
                if(last) begin packets <= packets + 1; capture_done <= 1; end
            end
            if(irq_ack != previous_ack) pending <= 0;
            if(irq_period == 0) begin timer <= 0; pending <= 0; end
            else if(timer >= irq_period - 1) begin
                timer <= 0; irq_count <= irq_count + 1; pending <= 1;
            end else timer <= timer + 1;

            if(tx_fire) begin
                tx_words <= tx_words + 1;
                if(upload) upload_address <= (upload_address + 1) & 4095;
                if(loopback || sink) begin
                    if(((s_axis_tdata ^ (expected_word ^ seed)) & valid_mask) != 0)
                        checker_errors <= checker_errors + 1;
                    if(s_axis_tkeep != 1 && s_axis_tkeep != 3 && s_axis_tkeep != 7 && s_axis_tkeep != 15)
                        keep_errors <= keep_errors + 1;
                    if((s_axis_tlast && packet_seen + input_bytes != packet_bytes) ||
                       (!s_axis_tlast && packet_seen + input_bytes >= packet_bytes))
                        discontinuities <= discontinuities + 1;
                    packet_seen <= s_axis_tlast ? 0 : packet_seen + input_bytes;
                    expected_word <= expected_word + 32'h9e3779b9;
                    input_index <= input_index + 1;
                end
                if(loopback) begin
                    valid <= 1; keep <= s_axis_tkeep; last <= s_axis_tlast;
                    data <= s_axis_tdata ^ ((fault[0] && input_index == 3) ? 1 : 0);
                end
            end

            if(capture != previous_capture && (play || generate_packet)) begin
                remaining <= play ? samples : packet_bytes;
                divider <= 0; capture_done <= 0; source_word <= 0; source_index <= 0;
            end else if(generate_packet && remaining != 0 && room && gate_out) begin
                valid <= 1;
                data <= (source_word ^ seed) ^ ((fault[0] && source_index == 3) ? 1 : 0);
                keep <= remaining >= 4 ? 15 : remaining == 3 ? 7 : remaining == 2 ? 3 : 1;
                last <= ((remaining <= 4) && !fault[2]) || (fault[1] && source_index == 3);
                remaining <= (remaining <= 4 || (fault[1] && source_index == 3)) ? 0 : remaining - 4;
                source_word <= source_word + 32'h9e3779b9; source_index <= source_index + 1;
            end else if(play && remaining != 0) begin
                if(divider == 0) begin
                    divider <= decimation > 0 ? decimation - 1 : 0;
                    if(room) begin
                        valid <= 1; data <= {2'b0,adc1,2'b0,adc0}; keep <= 15;
                        last <= remaining == 1; remaining <= remaining - 1;
                    end else overflow <= overflow + 1;
                end else divider <= divider - 1;
            end
            if(reset_diag != previous_reset) begin
                tx_words <= 0; rx_words <= 0; overflow <= 0; packets <= 0;
                discontinuities <= 0; checker_errors <= 0; keep_errors <= 0;
                input_stalls <= 0; output_stalls <= 0;
                expected_word <= 0; input_index <= 0; packet_seen <= 0;
            end
            if(snapshot != previous_snapshot) begin
                cycles_snapshot <= cycles; tx_snapshot <= tx_words; rx_snapshot <= rx_words;
                overflow_snapshot <= overflow; packets_snapshot <= packets;
                discontinuities_snapshot <= discontinuities; checker_errors_snapshot <= checker_errors;
                keep_errors_snapshot <= keep_errors; input_stalls_snapshot <= input_stalls;
                output_stalls_snapshot <= output_stalls; irq_count_snapshot <= irq_count;
                snapshot_id <= snapshot;
            end
        end
    end
endmodule
