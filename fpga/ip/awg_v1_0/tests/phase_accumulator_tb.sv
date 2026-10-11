`timescale 1ns / 1ps

module phase_accumulator_tb;
    reg passed = 0;
    reg clk = 0, resetn = 0, restart = 0, checking = 0;
    reg [47:0] a = 0, b = 0;
    always #2 clk = ~clk;

    genvar width;
    generate for (width = 32; width <= 48; width = width+1) begin : g_width
        wire carry;
        wire [width:0] expected_sum = {1'b0, a[width-1:0]} + {1'b0, b[width-1:0]};
        pm_phase_wrap #(.WIDTH(width)) wrap (.a(a[width-1:0]), .b(b[width-1:0]), .carry(carry));

        wire [7:0] lut_phase;
        wire bpsk;
        wire signed [16:0] gaussian;
        pm_waveform #(.PHASE_WIDTH(width), .MOD_WIDTH(16), .LUT_BITS(8), .SOURCE_MASK(640)) waveform (
            .clk(clk), .resetn(resetn), .restart(restart),
            .increment(a[width-1:0]), .phase_offset(b[width-1:0]), .duty({(width+1){1'b0}}),
            .seed(b[31:0]), .shape(4'd7), .lut_phase(lut_phase), .value(gaussian), .bpsk_state(bpsk)
        );
        reg [width-1:0] reference_phase = 0;
        reg reference_tick = 0, reference_bpsk = 0;
        wire [width:0] reference_next = {1'b0, reference_phase} + {1'b0, a[width-1:0]};
        wire [width-1:0] reference_offset = (restart ? {width{1'b0}} : reference_phase) + b[width-1:0];
        reg [31:0] reference_rng [0:11];
        reg signed [13:0] reference_noise [0:4];
        wire signed [16:0] expected_gaussian = reference_noise[4] * 8;
        reg [31:0] initial_rng, selected_rng;
        integer noise_sum, rng_index, stage, warmup = 0;
        always @(posedge clk) begin
            if (!resetn) reference_phase <= 0;
            else if (restart) reference_phase <= a[width-1:0];
            else reference_phase <= reference_next[width-1:0];
            reference_tick <= (!resetn || restart) ? 1'b0 : reference_next[width];
            if (!resetn || restart) reference_bpsk <= 0;
            else if (reference_tick) reference_bpsk <= ~reference_bpsk;
            // Independent unbalanced sum of the twelve selected bytes. The
            // reference retains the original 2*sum-3060 equation and latency.
            noise_sum = -3060;
            for (rng_index = 0; rng_index < 12; rng_index = rng_index+1) begin
                initial_rng = b[31:0] ^ (32'h9e3779b9 * (rng_index+1));
                if (initial_rng == 0) initial_rng = 1;
                selected_rng = restart ? initial_rng :
                    (reference_tick ? random_next32(reference_rng[rng_index]) : reference_rng[rng_index]);
                noise_sum = noise_sum + 2*selected_rng[31:24];
                if (!resetn) reference_rng[rng_index] <= 32'h9e3779b9 * (rng_index+1);
                else if (restart) reference_rng[rng_index] <= initial_rng;
                else if (reference_tick) reference_rng[rng_index] <= random_next32(reference_rng[rng_index]);
            end
            reference_noise[0] <= noise_sum;
            for (stage = 1; stage < 5; stage = stage+1) reference_noise[stage] <= reference_noise[stage-1];
            if (warmup < 6) warmup <= warmup+1;
        end
        always @(negedge clk) begin
            #1;
            if (checking) begin
                if (carry !== expected_sum[width]) $fatal(1, "Wrap carry width=%0d a=%h b=%h", width, a, b);
                if (waveform.phase !== reference_phase) $fatal(1, "Accumulator width=%0d", width);
                if (waveform.tick_pending !== reference_tick) $fatal(1, "Wrap timing width=%0d", width);
                if (lut_phase !== reference_offset[width-1 -: 8]) $fatal(1, "Offset/restart width=%0d", width);
                if (bpsk !== (restart ? 1'b0 : (reference_bpsk ^ reference_tick)))
                    $fatal(1, "BPSK timing width=%0d", width);
                if (warmup == 6 && gaussian !== expected_gaussian)
                    $fatal(1, "Gaussian timing width=%0d got=%d expected=%d", width, gaussian, expected_gaussian);
            end
        end
    end endgenerate

    task automatic step(input [47:0] left, right, input bit reset_level = 1, restart_level = 0);
        @(negedge clk);
        a = left; b = right; resetn = reset_level; restart = restart_level;
    endtask
    function automatic [63:0] random_next(input [63:0] state);
        reg [63:0] value;
        begin
            value = state ^ (state << 13);
            value = value ^ (value >> 7);
            random_next = value ^ (value << 17);
        end
    endfunction
    function automatic [31:0] random_next32(input [31:0] state);
        reg [31:0] value;
        begin
            value = state ^ (state << 13);
            value = value ^ (value >> 17);
            random_next32 = value ^ (value << 5);
        end
    endfunction
    reg [63:0] random_state = 64'hd35d942f84a67be1;
    reg [47:0] boundary, random_left;
    integer bit_index, cycle;
    initial begin
        repeat (3) @(negedge clk);
        checking = 1;
        step(0, 0);
        step('1, 0);
        step('1, 1);
        step('1, '1);
        step(1, 48'h9e3779b9, 1, 1); // Gaussian zero-mixed-state fallback.
        // Exercise generation and propagation across every bit and partial block.
        for (bit_index = 0; bit_index < 48; bit_index = bit_index+1) begin
            boundary = 48'd1 << bit_index;
            step(boundary-1, 1);
            step(boundary, boundary);
            step(~boundary, boundary);
            step(boundary-1, ~boundary);
        end
        for (cycle = 0; cycle < 20000; cycle = cycle+1) begin
            random_state = random_next(random_state);
            random_left = random_state[47:0];
            random_state = random_next(random_state);
            step(random_left, random_state[47:0], cycle % 4093 > 1, cycle % 701 == 0);
        end
        step(0, 0, 0, 1);
        step('1, '1, 1, 1);
        step(0, 0);
        repeat (3) @(negedge clk);
        passed = 1;
        $display("PASS: wrap carry, accumulator, offset, BPSK and Gaussian timing at every width 32-48");
        $finish;
    end
    initial begin #1000000; $fatal(1, "Phase adder test timed out"); end
endmodule
