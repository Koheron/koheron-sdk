`timescale 1ns / 1ps

// Each bank is owned from the first input beat until its final output beat
// is copied into the wrapper's elastic register.
// A bank is published only after its entire validated sum has been written.
module axis_accumulator_control #(
    parameter integer FRAME_LENGTH = 8192,
    parameter integer N_FRAMES = 1023,
    parameter integer CHECK_TLAST = 1,
    parameter integer SYNC_ON_RESET = 0,
    parameter integer ADD_LATENCY = 8
) (
    input wire aclk, aresetn,
    input wire [31:0] s_axis_tdata,
    input wire s_axis_tvalid,
    output wire s_axis_tready,
    input wire s_axis_tlast,
    output wire [31:0] m_axis_tdata,
    output reg m_axis_tvalid,
    input wire m_axis_tready,
    output reg m_axis_tlast,
    output reg [31:0] m_axis_tuser,
    output wire [31:0] frame_index,
    output wire [31:0] cycle_index,
    output reg [31:0] result_count,
    input wire result_done,
    output reg frame_error,
    output wire [31:0] add_a, add_b,
    output wire add_valid,
    input wire [31:0] add_result,
    input wire result_valid
);
    localparam integer ADDR_WIDTH = FRAME_LENGTH > 1 ? $clog2(FRAME_LENGTH) : 1;
    localparam integer FRAME_WIDTH = N_FRAMES > 1 ? $clog2(N_FRAMES) : 1;
    localparam integer PIPE_LENGTH = ADD_LATENCY + 2; // RAM read + operand register + adder
    reg [ADDR_WIDTH-1:0] input_bin;
    reg [FRAME_WIDTH-1:0] frame_count;
    assign frame_index = frame_count;
    reg input_bank, group_started, discarding;
    reg [1:0] allocated, ready_bank;
    reg [PIPE_LENGTH-1:0] pipe_valid, pipe_bank, pipe_end;
    reg [ADDR_WIDTH-1:0] pipe_bin [0:PIPE_LENGTH-1];
    reg [31:0] operand;
    reg operand_first, operand_bank;
    reg [31:0] registered_a, registered_b;
    wire [31:0] bank_data [0:1];

    // Conservative RAW interlock also supports frames shorter than the adder
    // latency. At ordinary FFT lengths it never interrupts a continuous input.
    wire hazard;
    integer stage;
    generate if (FRAME_LENGTH <= PIPE_LENGTH) begin : g_short_frame
        reg conflict;
        integer j;
        always @* begin
            conflict = 0;
            for (j=0; j<PIPE_LENGTH; j=j+1)
                if (pipe_valid[j] && pipe_bank[j] == input_bank && pipe_bin[j] == input_bin)
                    conflict = 1;
        end
        assign hazard = conflict;
    end else begin : g_long_frame
        // In normal frames a bin cannot repeat before writeback. Error
        // recovery below drains the pipeline before restarting at bin zero.
        assign hazard = 0;
    end endgenerate
    reg recovery_wait;
    assign s_axis_tready = aresetn && (group_started || !allocated[input_bank]) && !hazard && !recovery_wait;
    wire accept = s_axis_tvalid && s_axis_tready;
    wire expected_last = input_bin == FRAME_LENGTH-1;
    wire input_last = CHECK_TLAST ? s_axis_tlast : expected_last;
    wire bad_frame = accept && !discarding && (input_last != expected_last);
    wire enqueue = accept && !discarding && !bad_frame;
    wire end_group = enqueue && expected_last && frame_index == N_FRAMES-1;
    wire writeback = pipe_valid[PIPE_LENGTH-1] && result_valid && aresetn;

    reg output_bank, held_bank;
    // Includes FRAME_LENGTH as a sentinel: no read after the final prefetched bin.
    reg [ADDR_WIDTH:0] output_bin;
    wire output_done = m_axis_tvalid && m_axis_tready && m_axis_tlast;
    reg [1:0] pending_results;
    // Legacy BRAM readers wait for this progress value to wrap. Hold it at the
    // final frame until the result has actually drained into their recorder.
    assign cycle_index = pending_results != 0 ? N_FRAMES-1 : frame_index;
    wire fetch = aresetn && ready_bank[output_bank] && output_bin < FRAME_LENGTH &&
                 (!m_axis_tvalid || m_axis_tready);
    assign m_axis_tdata = bank_data[held_bank];
    // Isolate the BRAM output/bank mux from the first floating-point stage.
    assign add_a = registered_a;
    assign add_b = registered_b;
    assign add_valid = pipe_valid[1] && aresetn;

    // Each bank has one synchronous read port and one write port. Accumulation
    // and output cannot read the same bank because ownership is exclusive.
    genvar bank;
    generate for (bank=0; bank<2; bank=bank+1) begin : g_bank
        (* ram_style = "block" *) reg [31:0] memory [0:FRAME_LENGTH-1];
        reg [31:0] read_data;
        wire output_read = fetch && output_bank == bank;
        // Reading an invalid-frame beat is harmless: no adder tag is issued.
        // Keep frame-length/TLAST decoding out of the high-fanout RAM enable.
        wire accum_read = accept && !discarding && input_bank == bank;
        // Ownership selects the address; fetch only controls read enable.
        // This removes the output scheduling logic from the RAM address path.
        wire [ADDR_WIDTH-1:0] read_address = ready_bank[bank] ? output_bin[ADDR_WIDTH-1:0] : input_bin;
        always @(posedge aclk) begin
            if (output_read || accum_read) read_data <= memory[read_address];
            if (writeback && pipe_bank[PIPE_LENGTH-1] == bank)
                memory[pipe_bin[PIPE_LENGTH-1]] <= add_result;
        end
        assign bank_data[bank] = read_data;
    end endgenerate

    always @(posedge aclk) begin
        if (!aresetn) begin
            input_bin <= 0; frame_count <= 0; input_bank <= 0;
            group_started <= 0; discarding <= SYNC_ON_RESET; frame_error <= 0;
            recovery_wait <= 0;
            allocated <= 0; ready_bank <= 0;
            pending_results <= 0; result_count <= 0;
            pipe_valid <= 0; pipe_bank <= 0; pipe_end <= 0;
            operand <= 0; operand_first <= 0; operand_bank <= 0;
            registered_a <= 0; registered_b <= 0;
            output_bank <= 0; held_bank <= 0; output_bin <= 0;
            m_axis_tvalid <= 0; m_axis_tlast <= 0; m_axis_tuser <= 0;
            for (stage=0; stage<PIPE_LENGTH; stage=stage+1) pipe_bin[stage] <= 0;
        end else begin
            if (pipe_valid[0]) begin
                registered_a <= operand_first ? 32'b0 : bank_data[operand_bank];
                registered_b <= operand;
            end
            if (recovery_wait && pipe_valid == 0) recovery_wait <= 0;
            pipe_valid <= {pipe_valid[PIPE_LENGTH-2:0], enqueue};
            pipe_bank <= {pipe_bank[PIPE_LENGTH-2:0], input_bank};
            pipe_end <= {pipe_end[PIPE_LENGTH-2:0], end_group};
            pipe_bin[0] <= input_bin;
            for (stage=1; stage<PIPE_LENGTH; stage=stage+1) pipe_bin[stage] <= pipe_bin[stage-1];
            if (enqueue) begin
                operand <= s_axis_tdata;
                operand_first <= frame_index == 0;
                operand_bank <= input_bank;
            end
            if (accept) begin
                allocated[input_bank] <= 1;
                group_started <= 1;
                if (bad_frame || discarding) begin
                    // Discard the whole partial group. For late/missing TLAST,
                    // consume through the next TLAST before starting a new group.
                    if (bad_frame) frame_error <= 1;
                    input_bin <= 0; frame_count <= 0;
                    if (bad_frame) recovery_wait <= 1;
                    discarding <= !input_last;
                end else if (expected_last) begin
                    input_bin <= 0;
                    if (end_group) begin
                        input_bank <= !input_bank;
                        frame_count <= 0; group_started <= 0;
                    end else frame_count <= frame_count + 1;
                end else input_bin <= input_bin + 1;
            end
            if (writeback && pipe_end[PIPE_LENGTH-1]) ready_bank[pipe_bank[PIPE_LENGTH-1]] <= 1;
            case ({end_group, result_done})
                2'b10: pending_results <= pending_results + 1;
                2'b01: pending_results <= pending_results - 1;
                default: ;
            endcase
            if (m_axis_tvalid && m_axis_tready) m_axis_tvalid <= 0;
            if (fetch) begin
                held_bank <= output_bank;
                m_axis_tvalid <= 1;
                m_axis_tlast <= output_bin == FRAME_LENGTH-1;
                m_axis_tuser <= output_bin;
                output_bin <= output_bin + 1;
            end
            if (output_done) begin
                allocated[output_bank] <= 0; ready_bank[output_bank] <= 0;
                output_bank <= !output_bank; output_bin <= 0;
            end
            if (result_done) result_count <= result_count + 1;
        end
    end
endmodule
