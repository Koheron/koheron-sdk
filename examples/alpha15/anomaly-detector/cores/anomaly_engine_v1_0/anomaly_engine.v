`timescale 1ns/1ps
// Four ADC delays feed eight fixed ReLU units (positive and negative halves).
// Software trains the eight output weights and the bias. All samples are scored;
// the score appears two 240 MHz clocks (8.33 ns) after adc_valid.
module anomaly_engine (
    input wire clk, input wire resetn,
    input wire [17:0] sample, input wire sample_valid,
    input wire capture_enable, input wire arm, input wire model_enable,
    input wire [31:0] threshold, input wire commit,
    input wire [31:0] weight0, weight1, weight2, weight3,
    input wire [31:0] bias,
    output wire [31:0] tagged_sample, output wire tagged_valid,
    output reg alert = 0, output reg [31:0] score = 0,
    output reg [31:0] trigger_sequence = 0,
    output reg triggered = 0, output reg capture_done = 0,
    output reg [31:0] sequence_status = 0
);
    localparam integer PRE_SAMPLES = 15000000;
    localparam integer POST_SAMPLES = 15000000;
    reg [31:0] sequence = 0;
    reg [5:0] stream_startup = 0;
    reg [31:0] post_count = 0;
    reg [23:0] history_count = 0;
    reg [17:0] history [0:3];
    // Q12 output weights fit 16 bits, so each 17x16 product uses one DSP.
    reg signed [15:0] w [0:7];
    reg signed [31:0] b = 0;
    reg commit_meta = 0, commit_sync = 0;
    reg commit_seen = 0;
    reg [5:0] settle = 0;
    reg signed [16:0] relu [0:7];
    reg signed [32:0] product [0:7];
    reg signed [33:0] pair_sum [0:3];
    reg signed [34:0] half_sum [0:1];
    reg signed [35:0] sum = 0;
    reg signed [31:0] estimate_q = 0;
    reg [17:0] prediction = 0;
    reg [17:0] observed = 0, expected = 0;
    reg [31:0] observed_sequence = 0;
    reg compare_valid = 0;
    reg signed [19:0] difference = 0;
    reg difference_valid = 0;
    reg [31:0] difference_sequence = 0;
    reg pending_trigger = 0;
    reg [31:0] pending_sequence = 0;
    reg signed [19:0] limit_pos = 0, limit_neg = 0;
    integer k;
    wire signed [19:0] abs_difference = difference[19] ? -difference : difference;
    wire [31:0] magnitude = {12'd0, abs_difference};
    wire over_threshold = (difference > limit_pos) || (difference < limit_neg);
    assign tagged_sample = {sequence[13:0], sample};
    assign tagged_valid = sample_valid && stream_startup == 32 && !capture_done;

    function signed [16:0] feature;
        input [17:0] previous;
        input negative;
        reg signed [17:0] signed_sample;
        begin
            signed_sample = $signed(previous);
            if (negative)
                feature = signed_sample[17] ? -(signed_sample >>> 2) : 0;
            else
                feature = signed_sample[17] ? 0 : (signed_sample >>> 2);
        end
    endfunction

    always @(posedge clk) begin
        if (!resetn) begin
            sequence <= 0;
            stream_startup <= 0;
            post_count <= 0;
            history_count <= 0;
            triggered <= 0;
            capture_done <= 0;
            alert <= 0;
            score <= 0;
            compare_valid <= 0;
            difference_valid <= 0;
            pending_trigger <= 0;
            commit_meta <= 0;
            commit_sync <= 0;
            commit_seen <= 0;
            settle <= 0;
            for (k=0;k<4;k=k+1) history[k] <= 0;
            for (k=0;k<8;k=k+1) w[k] <= 0;
        end else begin
            // The control register is on a different clock; the weight bus
            // remains stable while this single-bit update crosses domains.
            commit_meta <= commit;
            commit_sync <= commit_meta;
            if (commit_sync != commit_seen) begin
                commit_seen <= commit_sync;
                settle <= 32; // Drain old products before allowing a new alert.
                w[0] <= weight0[15:0]; w[1] <= weight0[31:16];
                w[2] <= weight1[15:0]; w[3] <= weight1[31:16];
                w[4] <= weight2[15:0]; w[5] <= weight2[31:16];
                w[6] <= weight3[15:0]; w[7] <= weight3[31:16];
                b <= bias;
            end else if (settle != 0) settle <= settle - 1'b1;
            limit_pos <= $signed({2'b00,threshold[17:0]});
            limit_neg <= -$signed({2'b00,threshold[17:0]});
            if (!capture_enable) begin
                sequence <= 0;
                stream_startup <= 0;
                post_count <= 0;
                history_count <= 0;
                triggered <= 0;
                capture_done <= 0;
                pending_trigger <= 0;
            end else if (stream_startup < 32) begin
                stream_startup <= stream_startup + 1'b1;
            end else if (sample_valid && !capture_done) begin
                sequence <= sequence + 1'b1;
                if (history_count < PRE_SAMPLES) history_count <= history_count + 1'b1;
                sequence_status <= sequence + 1'b1;
                if (triggered) begin
                    post_count <= post_count + 1'b1;
                    if (post_count >= POST_SAMPLES && sequence[16:0] == 17'h1ffff)
                        capture_done <= 1;
                end
            end
            if (sample_valid) begin
                history[3] <= history[2]; history[2] <= history[1];
                history[1] <= history[0]; history[0] <= sample;
                observed <= sample;
                expected <= prediction;
                observed_sequence <= sequence;
            end
            compare_valid <= sample_valid;
            if (compare_valid) begin
                difference <= $signed(observed) - $signed(expected);
                difference_sequence <= observed_sequence;
            end
            difference_valid <= compare_valid;
            if (difference_valid) begin
                score <= magnitude;
                alert <= model_enable && settle == 0 && over_threshold;
                pending_trigger <= capture_enable && arm && !triggered &&
                    model_enable && settle == 0 && history_count >= PRE_SAMPLES &&
                    over_threshold;
                pending_sequence <= difference_sequence;
            end else begin
                pending_trigger <= 0;
            end
            if (capture_enable && pending_trigger && !triggered) begin
                triggered <= 1;
                trigger_sequence <= pending_sequence;
            end
            // Valid conversions are 16 fabric clocks apart. The feature,
            // multiply and adder-tree registers let each stage meet 240 MHz.
            for (k=0;k<4;k=k+1) begin
                relu[2*k] <= feature(history[k],1'b0);
                relu[2*k+1] <= feature(history[k],1'b1);
            end
            for (k=0;k<8;k=k+1) product[k] <= relu[k] * w[k];
            for (k=0;k<4;k=k+1)
                pair_sum[k] <= product[2*k] + product[2*k+1];
            half_sum[0] <= pair_sum[0] + pair_sum[1];
            half_sum[1] <= pair_sum[2] + pair_sum[3];
            sum <= half_sum[0] + half_sum[1];
            estimate_q <= ($signed(sum) >>> 12) + b;
            if (estimate_q < -32768) prediction <= 18'h20000;
            else if (estimate_q > 32767) prediction <= 18'h1ffff;
            else prediction <= {estimate_q[15:0], 2'b00};
        end
    end
endmodule
