`timescale 1 ns / 1 ps

// Phase-only vectoring extractor: eight CORDIC rotations plus small-angle
// completion by default, with a full-CORDIC fallback. The output uses the scaled-radian
// convention: pi = 2^(PHASE_WIDTH-3). One sample per clock.
// Normalization retains precision even for one-count IQ inputs. Two headroom
// bits accommodate vector magnitude and CORDIC gain without saturation.
module phase_extractor #(
    parameter integer INPUT_WIDTH = 24,
    parameter integer PHASE_WIDTH = 24,
    parameter integer ITERATIONS = PHASE_WIDTH,
    parameter integer ROTATIONS_PER_CLOCK = 2,
    parameter integer PAIR_START = 8,
    parameter integer FUSE_ROUND = 1,
    parameter integer COMPACT_PREP = 0,
    parameter integer RESIDUAL_CORRECTION = 1
)(
    input wire clk,
    input wire resetn,
    input wire valid_in,
    input wire signed [INPUT_WIDTH-1:0] i_in,
    input wire signed [INPUT_WIDTH-1:0] q_in,
    output wire valid_out,
    output wire signed [PHASE_WIDTH-1:0] phase_out
);
    initial begin
        if (PHASE_WIDTH != 16 && PHASE_WIDTH != 24) $error("Supported phase widths are 16 and 24");
        if (RESIDUAL_CORRECTION && (INPUT_WIDTH != 24 || PHASE_WIDTH != 24 || PAIR_START != 8 || COMPACT_PREP))
            $error("Residual completion requires 24-bit IQ/phase and three-stage preparation");
        if (ITERATIONS < 16 || ITERATIONS > 24) $error("Supported rotation counts are 16 through 24");
    end
    localparam integer N = RESIDUAL_CORRECTION ? 8 : ITERATIONS;
    localparam integer A = PHASE_WIDTH + 8;
    localparam signed [A-1:0] PI = 1 << (A-3);
    // Preserve every Cartesian input bit during normalization. Two magnitude
    // headroom bits and a sign bit accommodate the vectoring gain. The legacy
    // 16-bit interface retains its original 24-bit coordinate arithmetic.
    localparam integer INPUT_COORD_WIDTH = INPUT_WIDTH > 21 ? INPUT_WIDTH + 3 : 24;
    localparam integer W = PHASE_WIDTH+3 > INPUT_COORD_WIDTH ? PHASE_WIDTH+3 : INPUT_COORD_WIDTH;
    localparam integer GROUPS = PAIR_START + (N - PAIR_START + ROTATIONS_PER_CLOCK - 1) / ROTATIONS_PER_CLOCK;
    // Residual completion selects 14 clocks: three preparation stages, eight
    // vectoring stages and three small-angle correction stages. The full
    // 24-rotation fallback takes 19 clocks. Compact preparation is experimental.
    wire signed [INPUT_WIDTH:0] extended_i = {i_in[INPUT_WIDTH-1], i_in};
    wire signed [INPUT_WIDTH:0] extended_q = {q_in[INPUT_WIDTH-1], q_in};
    reg signed [INPUT_WIDTH:0] folded_x = 0, folded_y = 0;
    reg signed [A-1:0] quadrant = 0;
    reg zero0 = 1, valid0 = 0;
    wire signed [INPUT_WIDTH:0] prepared_x, prepared_y;
    wire signed [A-1:0] prepared_quadrant;
    wire [4:0] prepared_shift;
    wire prepared_zero, prepared_valid;

    function [4:0] normalization_shift;
        input [INPUT_WIDTH-1:0] m;
        integer bit_index;
        begin
            normalization_shift = W-4;
            for (bit_index = 0; bit_index < INPUT_WIDTH; bit_index = bit_index + 1)
                if (m[bit_index]) normalization_shift = W-4 - bit_index;
        end
    endfunction

    always @(posedge clk) begin
        folded_x <= i_in[INPUT_WIDTH-1] ? -extended_i : extended_i;
        folded_y <= i_in[INPUT_WIDTH-1] ? -extended_q : extended_q;
        quadrant <= i_in[INPUT_WIDTH-1] ? (q_in[INPUT_WIDTH-1] ? -PI : PI) : 0;
        zero0 <= (i_in == 0 && q_in == 0);
        valid0 <= resetn && valid_in;
    end

    generate if (COMPACT_PREP) begin : compact_prep
        // Compare abs(I/Q) with each power of two directly from signed
        // input bits. For negative x, abs(x) >= 2^b iff the upper bits are
        // not all ones, or all lower bits are zero. This handles -2^b
        // without waiting for a negation carry chain.
        wire [INPUT_WIDTH-1:0] at_least;
        for (genvar b=0; b<INPUT_WIDTH; b=b+1) begin : threshold
            wire lower_i_zero, lower_q_zero;
            if (b == 0) begin : no_lower_bits
                assign lower_i_zero = 1'b1;
                assign lower_q_zero = 1'b1;
            end else begin : lower_bits
                assign lower_i_zero = ~(|i_in[b-1:0]);
                assign lower_q_zero = ~(|q_in[b-1:0]);
            end
            wire i_at_least = i_in[INPUT_WIDTH-1] ? (~(&i_in[INPUT_WIDTH-1:b]) | lower_i_zero) : |i_in[INPUT_WIDTH-1:b];
            wire q_at_least = q_in[INPUT_WIDTH-1] ? (~(&q_in[INPUT_WIDTH-1:b]) | lower_q_zero) : |q_in[INPUT_WIDTH-1:b];
            assign at_least[b] = i_at_least | q_at_least;
        end
        // Decode contiguous ranges of leading positions directly from the
        // thermometer thresholds. Higher shift bits need fewer ranges than
        // a full one-hot decoder, reducing prefix fanout and logic depth.
        wire [4:0] next_shift;
        for (genvar s=0; s<5; s=s+1) begin : shift_bit
            wire [INPUT_WIDTH-1:0] selected;
            for (genvar b=0; b<INPUT_WIDTH; b=b+1) begin : select_range
                localparam integer VALUE = W-4-b;
                localparam integer LAST = b + (VALUE & ((1<<s)-1));
                if (((VALUE >> s) & 1) && (b == 0 || !(((VALUE+1) >> s) & 1))) begin : active
                    wire lower, upper;
                    if (b == 0) assign lower = 1'b1;
                    else assign lower = at_least[b];
                    if (LAST >= INPUT_WIDTH-1) assign upper = 1'b0;
                    else assign upper = at_least[LAST+1];
                    assign selected[b] = lower & ~upper;
                end else begin : inactive
                    assign selected[b] = 1'b0;
                end
            end
            assign next_shift[s] = |selected;
        end
        reg [4:0] shift = 0;
        always @(posedge clk) shift <= next_shift;
        assign prepared_x = folded_x;
        assign prepared_y = folded_y;
        assign prepared_quadrant = quadrant;
        assign prepared_shift = shift;
        assign prepared_zero = zero0;
        assign prepared_valid = valid0;
    end else begin : legacy_prep
        wire [INPUT_WIDTH:0] abs_i = i_in[INPUT_WIDTH-1] ? -extended_i : extended_i;
        wire [INPUT_WIDTH:0] abs_q = q_in[INPUT_WIDTH-1] ? -extended_q : extended_q;
        reg [INPUT_WIDTH-1:0] magnitude = 0;
        reg signed [INPUT_WIDTH:0] saved_x = 0, saved_y = 0;
        reg signed [A-1:0] saved_quadrant = 0;
        reg [4:0] shift = 0;
        reg zero1 = 1, valid1 = 0;
        always @(posedge clk) begin
            magnitude <= abs_i[INPUT_WIDTH-1:0] | abs_q[INPUT_WIDTH-1:0];
            saved_x <= folded_x;
            saved_y <= folded_y;
            saved_quadrant <= quadrant;
            shift <= normalization_shift(magnitude);
            zero1 <= zero0;
            valid1 <= resetn && valid0;
        end
        assign prepared_x = saved_x;
        assign prepared_y = saved_y;
        assign prepared_quadrant = saved_quadrant;
        assign prepared_shift = shift;
        assign prepared_zero = zero1;
        assign prepared_valid = valid1;
    end endgenerate

    // Add the final rotation and the rounding bias in the same carry chain.
    // Taking the retained phase bits also applies the canonical modulo-2*pi phase cut.
    function [PHASE_WIDTH-3:0] round_rotation;
        input signed [A-1:0] phase;
        input signed [A-1:0] rotation_angle;
        reg signed [A-1:0] biased;
        begin
            biased = phase + rotation_angle + 128;
            round_rotation = biased[A-3:8];
        end
    endfunction

    // Three-term carry-save sum with the 0/1/2 corrections needed when
    // complementing operands for subtraction. The final carry-in is only
    // one bit; no extra full-width correction adder is required.
    function [W-1:0] vector_sum;
        input [W-1:0] a, b, c;
        input [1:0] correction;
        reg [W-1:0] sum, carry;
        reg [W-2:0] upper;
        begin
            sum = a ^ b ^ c;
            carry = ((a & b) | (a & c) | (b & c)) << 1;
            case(correction)
                1: begin
                    upper = sum[W-1:1] + carry[W-1:1] + sum[0];
                    vector_sum = {upper, ~sum[0]};
                end
                2: begin
                    upper = sum[W-1:1] + carry[W-1:1] + 1'b1;
                    vector_sum = {upper, sum[0]};
                end
                default: vector_sum = sum + carry;
            endcase
        end
    endfunction

    function signed [A-1:0] angle;
        input integer index;
        reg [31:0] fine_angle;
        begin
            // round(atan(2^-index) / pi * 2^29). Eight internal fractional
            // bits beyond the selected output avoid accumulating LUT bias.
            case (index)
                0: fine_angle = 32'd134217728;
                1: fine_angle = 32'd79233351;
                2: fine_angle = 32'd41864727;
                3: fine_angle = 32'd21251189;
                4: fine_angle = 32'd10666833;
                5: fine_angle = 32'd5338616;
                6: fine_angle = 32'd2669960;
                7: fine_angle = 32'd1335061;
                8: fine_angle = 32'd667541;
                9: fine_angle = 32'd333772;
                10: fine_angle = 32'd166886;
                11: fine_angle = 32'd83443;
                12: fine_angle = 32'd41722;
                13: fine_angle = 32'd20861;
                14: fine_angle = 32'd10430;
                15: fine_angle = 32'd5215;
                16: fine_angle = 32'd2608;
                17: fine_angle = 32'd1304;
                18: fine_angle = 32'd652;
                19: fine_angle = 32'd326;
                20: fine_angle = 32'd163;
                21: fine_angle = 32'd81;
                22: fine_angle = 32'd41;
                23: fine_angle = 32'd20;
                default: fine_angle = 0;
            endcase
            if (PHASE_WIDTH == 24) angle = fine_angle;
            else angle = (fine_angle + 128) >> 8;
        end
    endfunction

    wire signed [W-1:0] x [0:N];
    wire signed [W-1:0] y [0:N];
    wire signed [A-1:0] z [0:N];
    reg signed [W-1:0] norm_x = 0, norm_y = 0;
    reg signed [A-1:0] norm_z = 0;
    reg [GROUPS:0] zeros = {(GROUPS+1){1'b1}};
    reg [GROUPS:0] valids = 0;
    always @(posedge clk) begin
        norm_x <= $signed({{(W-INPUT_WIDTH-1){prepared_x[INPUT_WIDTH]}}, prepared_x}) <<< prepared_shift;
        norm_y <= $signed({{(W-INPUT_WIDTH-1){prepared_y[INPUT_WIDTH]}}, prepared_y}) <<< prepared_shift;
        norm_z <= prepared_quadrant;
        zeros[0] <= prepared_zero;
        valids[0] <= resetn && prepared_valid;
    end
    assign x[0] = norm_x;
    assign y[0] = norm_y;
    assign z[0] = norm_z;

    genvar k;
    generate for (k = 0; k < N; k = k + 1) begin : rotation
        // Reduce the residual's width at every rotation, including rotations
        // sharing a clock. Otherwise an unnecessary sign carry remains on
        // the critical path between the two rotations.
        localparam integer Y_WIDTH = W - (k > 0 ? k-1 : 0);
        wire signed [W-1:0] next_x;
        if (k < 8) begin : update_x
            assign next_x = y[k][W-1] ? x[k] - (y[k] >>> k) : x[k] + (y[k] >>> k);
        end else begin : fixed_x
            // Once the remaining angle is below atan(2^-7), x changes by
            // less than 0.004%. Holding it removes the late full-width x
            // carry chains. The total angular error, including coordinate
            // truncation, is checked independently against atan2.
            assign next_x = x[k];
        end
        wire signed [Y_WIDTH-1:0] next_y;
        wire signed [A-1:0] next_z;
        if (k >= 9 && k >= PAIR_START && ROTATIONS_PER_CLOCK == 2 && (k-PAIR_START)%2 == 1) begin : speculative
            // The first rotation's sign arrives late. Select already computed
            // add/subtract results instead of driving a second carry input.
            wire [W-1:0] first_step = x[k-1] >>> (k-1);
            wire [W-1:0] second_step = x[k-1] >>> k;
            (* keep = "true" *) wire signed [Y_WIDTH-1:0] y_pp = vector_sum(y[k-1],~first_step,~second_step,2);
            (* keep = "true" *) wire signed [Y_WIDTH-1:0] y_pn = vector_sum(y[k-1],~first_step,second_step,1);
            (* keep = "true" *) wire signed [Y_WIDTH-1:0] y_np = vector_sum(y[k-1],first_step,~second_step,1);
            (* keep = "true" *) wire signed [Y_WIDTH-1:0] y_nn = vector_sum(y[k-1],first_step,second_step,0);
            // Accumulate both angles in one carry chain. Four speculative
            // constant sums keep either rotation's sign off that chain.
            localparam signed [A-1:0] BOTH = angle(k-1) + angle(k);
            localparam signed [A-1:0] DIFFERENCE = angle(k-1) - angle(k);
            (* keep = "true" *) wire signed [A-1:0] z_pp = z[k-1] + BOTH;
            (* keep = "true" *) wire signed [A-1:0] z_pn = z[k-1] + DIFFERENCE;
            (* keep = "true" *) wire signed [A-1:0] z_np = z[k-1] - DIFFERENCE;
            (* keep = "true" *) wire signed [A-1:0] z_nn = z[k-1] - BOTH;
            assign next_y = y[k-1][W-1] ? (y[k][W-1] ? y_nn : y_np) :
                                                   (y[k][W-1] ? y_pn : y_pp);
            assign next_z = y[k-1][W-1] ? (y[k][W-1] ? z_nn : z_np) :
                                                   (y[k][W-1] ? z_pn : z_pp);
        end else begin : ordinary
            assign next_y = y[k][W-1] ? y[k] + (x[k] >>> k) : y[k] - (x[k] >>> k);
            assign next_z = y[k][W-1] ? z[k] - angle(k) : z[k] + angle(k);
        end
        if (k == N-1 && FUSE_ROUND && !RESIDUAL_CORRECTION) begin : final_round
            wire [PHASE_WIDTH-3:0] rounded;
            if (k >= 9 && k >= PAIR_START && ROTATIONS_PER_CLOCK == 2 && (k-PAIR_START)%2 == 1) begin : paired
                localparam signed [A-1:0] BOTH = angle(k-1) + angle(k);
                localparam signed [A-1:0] DIFFERENCE = angle(k-1) - angle(k);
                wire [PHASE_WIDTH-3:0] p_pp = round_rotation(z[k-1], BOTH);
                wire [PHASE_WIDTH-3:0] p_pn = round_rotation(z[k-1], DIFFERENCE);
                wire [PHASE_WIDTH-3:0] p_np = round_rotation(z[k-1], -DIFFERENCE);
                wire [PHASE_WIDTH-3:0] p_nn = round_rotation(z[k-1], -BOTH);
                assign rounded = y[k-1][W-1] ? (y[k][W-1] ? p_nn : p_np) :
                                                           (y[k][W-1] ? p_pn : p_pp);
            end else begin : single
                wire [PHASE_WIDTH-3:0] p_positive = round_rotation(z[k], angle(k));
                wire [PHASE_WIDTH-3:0] p_negative = round_rotation(z[k], -angle(k));
                assign rounded = y[k][W-1] ? p_negative : p_positive;
            end
            reg signed [PHASE_WIDTH-1:0] phase_word = 0;
            reg phase_valid = 0;
            assign phase_out = phase_word;
            assign valid_out = phase_valid;
            always @(posedge clk) begin
                phase_word <= zeros[GROUPS-1] ? {PHASE_WIDTH{1'b0}} : {{2{rounded[PHASE_WIDTH-3]}},rounded};
                phase_valid <= resetn && valids[GROUPS-1];
            end
            assign x[k+1] = next_x;
            assign y[k+1] = next_y;
            assign z[k+1] = next_z;
        end else if (k < PAIR_START || (k + 1 - PAIR_START) % ROTATIONS_PER_CLOCK == 0 || k == N-1) begin : pipeline
            localparam integer GROUP = k < PAIR_START ? k+1 : PAIR_START + (k - PAIR_START) / ROTATIONS_PER_CLOCK + 1;
            // After iteration k the residual y is bounded by x * 2^-k.
            // One guard bit keeps rounding from violating that bound.
            reg signed [W-1:0] xr = 0;
            reg signed [Y_WIDTH-1:0] yr = 0;
            reg signed [A-1:0] zr = 0;
            always @(posedge clk) begin
                xr <= next_x;
                yr <= next_y;
                zr <= next_z;
                zeros[GROUP] <= zeros[GROUP-1];
                valids[GROUP] <= resetn && valids[GROUP-1];
            end
            assign x[k+1] = xr;
            assign y[k+1] = yr;
            assign z[k+1] = zr;
        end else begin : combinational
            assign x[k+1] = next_x;
            assign y[k+1] = next_y;
            assign z[k+1] = next_z;
        end
    end endgenerate

    // Nearest rounding, ties towards +infinity. Modulo 2*pi is just a
    // PHASE_WIDTH-2 bit slice; sign extension produces the canonical [-pi, pi) range.
    // Only bit 7 generates a carry into the retained fractional bits.
    generate if (!FUSE_ROUND && !RESIDUAL_CORRECTION) begin : registered_round
        wire signed [PHASE_WIDTH-3:0] rounded = z[N][A-3:8] + z[N][7];
        reg signed [PHASE_WIDTH-1:0] phase_word = 0;
        reg phase_valid = 0;
        assign phase_out = phase_word;
        assign valid_out = phase_valid;
        always @(posedge clk) begin
            phase_word <= zeros[GROUPS] ? {PHASE_WIDTH{1'b0}} : {{2{rounded[PHASE_WIDTH-3]}},rounded};
            phase_valid <= resetn && valids[GROUPS];
        end
    end endgenerate
    generate if (RESIDUAL_CORRECTION) begin : residual_completion
        phase_residual completion (
            .clk(clk), .resetn(resetn), .valid_in(valids[GROUPS]),
            .zero_in(zeros[GROUPS]), .x_in(x[N]), .y_in(y[N][20:0]),
            .angle_in(z[N]), .valid_out(valid_out), .phase_out(phase_out)
        );
    end endgenerate
endmodule
