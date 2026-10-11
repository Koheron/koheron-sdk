`timescale 1ns / 1ps

module scale_tb;
    reg passed = 0;
    reg clk = 0, resetn = 0, valid = 0;
    reg [47:0] increment = 0, base_phase = 0;
    reg [48:0] deviation = 0;
    reg signed [24:0] modulation = 0;
    reg [3:0] shape = 0;
    reg bpsk_state = 0, pm_enabled = 0, output_enabled = 0, restart = 0;
    always #2 clk = ~clk;

    genvar width, precision;
    generate for (width = 32; width <= 48; width = width+1) begin : g_width
        for (precision = 0; precision < 2; precision = precision+1) begin : g_precision
            localparam integer MOD_WIDTH = precision == 0 ? 16 : 24;
            localparam integer BYTES = (width+7)/8;
            localparam integer DATA_WIDTH = 2*BYTES*8+8;
            wire [DATA_WIDTH-1:0] tdata;
            wire tvalid, tuser;
            wire signed [MOD_WIDTH:0] sample = modulation[MOD_WIDTH:0];
            pm_scale #(.PHASE_WIDTH(width), .MOD_WIDTH(MOD_WIDTH)) dut (
                .clk(clk), .resetn(resetn), .valid(valid), .modulation(sample),
                .increment(increment[width-1:0]), .base_phase(base_phase[width-1:0]),
                .deviation(deviation[width:0]), .shape(shape), .bpsk_state(bpsk_state),
                .pm_enabled(pm_enabled), .output_enabled(output_enabled), .restart(restart),
                .tdata(tdata), .tvalid(tvalid), .tuser(tuser)
            );
            // Full-width mathematical reference, independent of the split
            // multipliers and arithmetic pipeline used by the implementation.
            reg signed [127:0] product;
            reg [width-1:0] offset;
            reg [DATA_WIDTH-1:0] expected [0:6];
            reg [6:0] expected_valid = 0, expected_user = 0;
            integer stage, warmup = 0;
            always @(posedge clk) begin
                product = $signed(sample) * $signed({1'b0, deviation[width:0]});
                product = (product + (128'sd1 << (MOD_WIDTH-2))) >>> (MOD_WIDTH-1);
                offset = product[width-1:0];
                if (shape == 9) offset = bpsk_state ? deviation[width-1:0] : 0;
                else if (shape == 1 || shape == 2 || shape == 8)
                    offset = sample[MOD_WIDTH] ? -deviation[width-1:0] : deviation[width-1:0];
                if (!pm_enabled) offset = 0;
                expected[0] <= 0;
                expected[0][0 +: width] <= restart ? 0 : increment[width-1:0];
                expected[0][BYTES*8 +: width] <= base_phase[width-1:0] + offset;
                expected[0][2*BYTES*8] <= restart;
                for (stage = 1; stage < 7; stage = stage+1) expected[stage] <= expected[stage-1];
                expected_user <= {expected_user[5:0], output_enabled};
                if (!resetn) expected_valid <= 0;
                else expected_valid <= {expected_valid[5:0], valid};
                if (warmup < 8) warmup <= warmup+1;
            end
            always @(negedge clk) begin
                #1;
                if (tvalid !== expected_valid[6]) $fatal(1, "Scaler valid width=%0d mod=%0d", width, MOD_WIDTH);
                if (warmup == 8) begin
                    if (tdata !== expected[6]) $fatal(1, "Scaler data width=%0d mod=%0d got=%h expected=%h", width, MOD_WIDTH, tdata, expected[6]);
                    if (tuser !== expected_user[6]) $fatal(1, "Scaler enable width=%0d mod=%0d", width, MOD_WIDTH);
                end
            end
        end
    end endgenerate

    reg [63:0] random_state = 64'h23b1d087b3e98a15;
    function automatic [63:0] random_next(input [63:0] state);
        reg [63:0] value;
        begin
            value = state ^ (state << 13);
            value = value ^ (value >> 7);
            random_next = value ^ (value << 17);
        end
    endfunction
    integer cycle, kind, extreme;
    initial begin
        repeat (3) @(negedge clk);
        resetn = 1; valid = 1; pm_enabled = 1; output_enabled = 1;
        // Exercise exact endpoints, all depth limbs and signed rounding.
        for (kind = 0; kind < 10; kind = kind+1) begin
            for (extreme = 0; extreme < 8; extreme = extreme+1) begin
                @(negedge clk);
                shape = kind; bpsk_state = extreme[0];
                modulation = extreme[0] ? 25'h1000000 : 25'h0ffffff;
                case (extreme / 2)
                    0: deviation = 0;
                    1: deviation = 1;
                    2: deviation = 49'h1000000000000;
                    3: deviation = '1;
                endcase
                base_phase = '1; increment = '1; restart = extreme[1];
            end
        end
        for (cycle = 0; cycle < 20000; cycle = cycle+1) begin
            @(negedge clk);
            random_state = random_next(random_state);
            increment = random_state[47:0]; modulation = random_state[24:0];
            shape = cycle % 10; bpsk_state = random_state[63];
            pm_enabled = random_state[62]; output_enabled = random_state[61]; valid = random_state[60];
            random_state = random_next(random_state);
            base_phase = random_state[47:0];
            random_state = random_next(random_state);
            deviation = random_state[48:0];
            resetn = cycle % 4093 > 1; restart = cycle % 701 == 0;
        end
        repeat (10) @(negedge clk);
        #1.5;
        passed = 1;
        $display("PASS: scaler arithmetic and seven-cycle data/valid/enable timing, phase widths 32-48, modulation widths 16/24");
        $finish;
    end
    initial begin #1000000; $fatal(1, "Scaler test timed out"); end
endmodule
