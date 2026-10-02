`timescale 1ns / 1ps

// Internal PM sources. Values use signed Q2.(MOD_WIDTH-1), allowing exact +/-1.
module pm_waveform #(
    parameter integer PHASE_WIDTH = 48,
    parameter integer MOD_WIDTH = 24,
    parameter integer LUT_BITS = 14,
    parameter integer SOURCE_MASK = 1023,
    parameter integer PRBS_WIDTH = 31
) (
    input wire clk, resetn, restart,
    input wire [PHASE_WIDTH-1:0] increment, phase_offset,
    input wire [PHASE_WIDTH:0] duty,
    input wire [31:0] seed,
    input wire [3:0] shape,
    output wire [LUT_BITS-1:0] lut_phase,
    output wire signed [MOD_WIDTH:0] value,
    output wire bpsk_state
);
    localparam signed [MOD_WIDTH:0] ONE = (1 << (MOD_WIDTH-1));
    reg [PHASE_WIDTH-1:0] phase;
    wire [PHASE_WIDTH-1:0] sample_phase = (restart ? 0 : phase) + phase_offset;
    wire [PHASE_WIDTH:0] next_phase = {1'b0, phase} + {1'b0, increment};
    wire tick = next_phase[PHASE_WIDTH];
    // Register the 48-bit wrap detector before distributing it to noise/PRBS
    // enables. Predict the pending state below to preserve sample timing.
    (* max_fanout = 16 *) reg tick_pending;
    always @(posedge clk) begin
        if (!resetn || restart) tick_pending <= 0;
        else tick_pending <= tick;
    end
    assign lut_phase = sample_phase[PHASE_WIDTH-1 -: LUT_BITS];
    reg [PHASE_WIDTH-1:0] source_phase;
    reg [PHASE_WIDTH:0] source_duty;
    reg [3:0] source_shape;
    wire [MOD_WIDTH-1:0] ramp = source_phase[PHASE_WIDTH-1 -: MOD_WIDTH];
    wire signed [MOD_WIDTH+1:0] ramp_wide = $signed({2'b00, ramp});
    wire pulse_high = {1'b0, source_phase} < source_duty;

    always @(posedge clk) begin
        if (!resetn) phase <= 0;
        else if (restart) phase <= increment;
        else phase <= next_phase[PHASE_WIDTH-1:0];
    end

    generate if (SOURCE_MASK & 512) begin : g_bpsk
        reg state;
        always @(posedge clk) begin
            if (!resetn || restart) state <= 0;
            else if (tick_pending) state <= ~state;
        end
        assign bpsk_state = restart ? 1'b0 : (state ^ tick_pending);
    end else begin : g_no_bpsk
        assign bpsk_state = 0;
    end endgenerate

    wire prbs_bit;
    generate if (SOURCE_MASK & 256) begin : g_prbs
        localparam integer TAP = PRBS_WIDTH == 7 ? 6 :
                                 PRBS_WIDTH == 15 ? 14 :
                                 PRBS_WIDTH == 23 ? 18 : 28;
        reg [PRBS_WIDTH-1:0] state;
        wire [PRBS_WIDTH-1:0] initial_state = seed[PRBS_WIDTH-1:0];
        always @(posedge clk) begin
            if (!resetn) state <= 1;
            else if (restart) state <= initial_state;
            else if (tick_pending) state <= {state[PRBS_WIDTH-2:0], state[PRBS_WIDTH-1] ^ state[TAP-1]};
        end
        assign prbs_bit = restart ? initial_state[PRBS_WIDTH-1] :
            (tick_pending ? state[PRBS_WIDTH-2] : state[PRBS_WIDTH-1]);
    end else begin : g_no_prbs
        assign prbs_bit = 0;
    end endgenerate

    function [31:0] random_next;
        input [31:0] x;
        reg [31:0] y;
        begin
            y = x ^ (x << 13);
            y = y ^ (y >> 17);
            random_next = y ^ (y << 5);
        end
    endfunction

    wire signed [MOD_WIDTH:0] uniform_value;
    generate if (SOURCE_MASK & 64) begin : g_uniform
        reg [31:0] state;
        wire [31:0] selected = restart ? seed : (tick_pending ? random_next(state) : state);
        always @(posedge clk) begin
            if (!resetn) state <= 1;
            else if (restart) state <= seed;
            else if (tick_pending) state <= random_next(state);
        end
        assign uniform_value = $signed({1'b0, selected[31 -: MOD_WIDTH]}) - ONE;
    end else begin : g_no_uniform
        assign uniform_value = 0;
    end endgenerate

    // Sum twelve 8-bit uniforms: approximately Gaussian, sigma about 1/8,
    // bounded by +/-3060/4096. Independent deterministic xorshift states.
    wire signed [MOD_WIDTH:0] gaussian_value;
    generate if (SOURCE_MASK & 128) begin : g_gaussian
        wire [7:0] samples [0:11];
        genvar k;
        for (k = 0; k < 12; k = k+1) begin : g_rng
            reg [31:0] state;
            wire [31:0] mixed = seed ^ (32'h9e3779b9 * (k+1));
            wire [31:0] initial_state = mixed == 0 ? 32'd1 : mixed;
            wire [31:0] selected = restart ? initial_state : (tick_pending ? random_next(state) : state);
            always @(posedge clk) begin
                if (!resetn) state <= 32'h9e3779b9 * (k+1);
                else if (restart) state <= initial_state;
                else if (tick_pending) state <= random_next(state);
            end
            assign samples[k] = selected[31:24];
        end
        // Balanced adder tree avoids twelve serial carry chains at 250 MHz.
        reg [8:0] pairs [0:5];
        reg [9:0] quads [0:2];
        for (k = 0; k < 6; k = k+1) begin : g_pairs
            always @(posedge clk) pairs[k] <= {1'b0, samples[2*k]} + {1'b0, samples[2*k+1]};
        end
        for (k = 0; k < 3; k = k+1) begin : g_quads
            always @(posedge clk) quads[k] <= {1'b0, pairs[2*k]} + {1'b0, pairs[2*k+1]};
        end
        reg [10:0] sum01;
        reg [9:0] quad2;
        reg [11:0] total;
        reg signed [13:0] centered;
        always @(posedge clk) begin
            sum01 <= {1'b0, quads[0]} + {1'b0, quads[1]};
            quad2 <= quads[2];
            total <= {1'b0, sum01} + {2'b00, quad2};
            centered <= $signed({1'b0, total, 1'b0}) - 14'sd3060;
        end
        assign gaussian_value = centered * (1 << (MOD_WIDTH-13));
    end else begin : g_no_gaussian
        assign gaussian_value = 0;
    end endgenerate

    // Separate the wide phase-offset addition from comparison/ramp arithmetic.
    reg signed [MOD_WIDTH:0] source_uniform;
    reg source_prbs;
    always @(posedge clk) begin
        source_phase <= sample_phase;
        source_duty <= duty;
        source_shape <= shape;
        source_uniform <= uniform_value;
        source_prbs <= prbs_bit;
    end
    reg signed [MOD_WIDTH:0] immediate_value;
    always @* begin
        immediate_value = 0;
        case (source_shape)
            1: if (SOURCE_MASK & 2) immediate_value = source_phase[PHASE_WIDTH-1] ? -ONE : ONE;
            2: if (SOURCE_MASK & 4) immediate_value = pulse_high ? ONE : -ONE;
            3: if (SOURCE_MASK & 8) immediate_value = source_phase[PHASE_WIDTH-1] ?
                    3*ONE - (ramp_wide <<< 1) : (ramp_wide <<< 1) - ONE;
            4: if (SOURCE_MASK & 16) immediate_value = ramp_wide - ONE;
            5: if (SOURCE_MASK & 32) immediate_value = ONE - ramp_wide;
            6: if (SOURCE_MASK & 64) immediate_value = source_uniform;
            8: if (SOURCE_MASK & 256) immediate_value = source_prbs ? ONE : -ONE;
            default: immediate_value = 0; // Sine arrives through the Xilinx LUT stream.
        endcase
    end
    // Gaussian arithmetic takes five cycles. Delay the other sources equally;
    // the controller delays phase and settings by the same amount.
    reg signed [MOD_WIDTH:0] value_pipe [0:3];
    reg [3:0] shape_pipe [0:4];
    integer i;
    always @(posedge clk) begin
        value_pipe[0] <= immediate_value;
        shape_pipe[0] <= shape;
        for (i=1; i<5; i=i+1) begin
            if (i < 4) value_pipe[i] <= value_pipe[i-1];
            shape_pipe[i] <= shape_pipe[i-1];
        end
    end
    assign value = shape_pipe[4] == 7 ? gaussian_value : value_pipe[3];
endmodule
