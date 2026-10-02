`timescale 1ns / 1ps

// Three 17-bit deviation limbs fit DSP48E1's 25x18 multiplier at MOD_WIDTH=24.
// Pipeline data has no reset to permit DSP/SRL inference; validity is reset.
module pm_scale #(
    parameter integer PHASE_WIDTH = 48,
    parameter integer MOD_WIDTH = 24,
    parameter integer SOURCE_MASK = 1023,
    parameter integer PHASE_BYTES = (PHASE_WIDTH+7)/8
) (
    input wire clk, resetn, valid,
    input wire signed [MOD_WIDTH:0] modulation,
    input wire [PHASE_WIDTH-1:0] increment, base_phase,
    input wire [PHASE_WIDTH:0] deviation,
    input wire [3:0] shape,
    input wire bpsk_state, pm_enabled, output_enabled, restart,
    output reg [2*PHASE_BYTES*8+7:0] tdata,
    output wire tvalid,
    output reg [0:0] tuser
);
    localparam integer PRODUCT_WIDTH = PHASE_WIDTH + MOD_WIDTH + 3;
    localparam integer META_WIDTH = 2*PHASE_WIDTH + 3;
    reg [5:0] valid_pipe;
    reg [META_WIDTH-1:0] meta [0:5];
    integer i;
    always @(posedge clk) begin
        if (!resetn) valid_pipe <= 0;
        else valid_pipe <= {valid_pipe[4:0], valid};
        meta[0] <= {restart, output_enabled, pm_enabled, base_phase, increment};
        for (i=1; i<6; i=i+1) meta[i] <= meta[i-1];
    end
    // Output registers add one cycle to the six arithmetic stages.
    reg output_valid;
    always @(posedge clk) begin
        if (!resetn) output_valid <= 0;
        else output_valid <= valid_pipe[5];
    end
    assign tvalid = output_valid;

    wire [PHASE_WIDTH-1:0] scaled;
    generate if (SOURCE_MASK & 249) begin : g_scale
        wire [50:0] depth_padded = {{(50-PHASE_WIDTH){1'b0}}, deviation};
        (* use_dsp = "yes" *) reg signed [MOD_WIDTH+18:0] p0, p1, p2;
        reg signed [MOD_WIDTH+18:0] q0, q1, q2;
        reg signed [PRODUCT_WIDTH-1:0] sum01, high2, product, rounded;
        reg [PHASE_WIDTH-1:0] result;
        always @(posedge clk) begin
            p0 <= modulation * $signed({1'b0, depth_padded[16:0]});
            p1 <= modulation * $signed({1'b0, depth_padded[33:17]});
            p2 <= modulation * $signed({1'b0, depth_padded[50:34]});
            q0 <= p0; q1 <= p1; q2 <= p2;
            sum01 <= $signed(q0) + ($signed({{(PRODUCT_WIDTH-MOD_WIDTH-19){q1[MOD_WIDTH+18]}}, q1}) <<< 17);
            high2 <= $signed({{(PRODUCT_WIDTH-MOD_WIDTH-19){q2[MOD_WIDTH+18]}}, q2}) <<< 34;
            product <= sum01 + high2;
            rounded <= product + ({{(PRODUCT_WIDTH-1){1'b0}}, 1'b1} << (MOD_WIDTH-2));
            result <= rounded >>> (MOD_WIDTH-1);
        end
        assign scaled = result;
    end else begin : g_no_scale
        assign scaled = 0;
    end endgenerate

    // Exact endpoints and BPSK bypass the multiplier, including one-LSB depths.
    reg [PHASE_WIDTH-1:0] exact_phase [0:5];
    reg [5:0] exact_select;
    always @(posedge clk) begin
        exact_select[0] <= shape == 1 || shape == 2 || shape == 8 || shape == 9;
        if (shape == 9) exact_phase[0] <= bpsk_state ? deviation[PHASE_WIDTH-1:0] : 0;
        else exact_phase[0] <= modulation[MOD_WIDTH] ? -deviation[PHASE_WIDTH-1:0] : deviation[PHASE_WIDTH-1:0];
        for (i=1; i<6; i=i+1) begin
            exact_phase[i] <= exact_phase[i-1];
            exact_select[i] <= exact_select[i-1];
        end
    end

    wire [PHASE_WIDTH-1:0] offset = meta[5][2*PHASE_WIDTH] ?
        (exact_select[5] ? exact_phase[5] : scaled) : 0;
    always @(posedge clk) begin
        tdata <= 0;
        // RESYNC's first sample is PINC+POFF in the vendor DDS. Zero PINC on
        // that sample so restart begins at the requested phase, then advances.
        tdata[0 +: PHASE_WIDTH] <= meta[5][2*PHASE_WIDTH+2] ? 0 : meta[5][0 +: PHASE_WIDTH];
        tdata[PHASE_BYTES*8 +: PHASE_WIDTH] <= meta[5][PHASE_WIDTH +: PHASE_WIDTH] + offset;
        tdata[2*PHASE_BYTES*8] <= meta[5][2*PHASE_WIDTH+2]; // RESYNC byte
        tuser <= meta[5][2*PHASE_WIDTH+1]; // Output-enable travels through DDS.
    end
endmodule
