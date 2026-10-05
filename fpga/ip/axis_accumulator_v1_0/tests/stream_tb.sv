`timescale 1ns / 1ps
module stream_tb #(
    parameter FRAME_LENGTH = 64,
    parameter N_FRAMES = 3,
    parameter STALLS = 1,
    parameter CHECK_TLAST = 1
);
    reg passed = 0;
    reg clk = 0;
    always #2 clk = !clk;
    reg resetn = 0, valid = 0, last = 0, ready = 0;
    reg force_stall = 1;
    reg [31:0] data = 0;
    wire in_ready, out_valid, out_last, error;
    wire [31:0] sum, bin, frame, result_count;
    axis_accumulator #(.FRAME_LENGTH(FRAME_LENGTH), .N_FRAMES(N_FRAMES), .CHECK_TLAST(CHECK_TLAST)) dut (
        .aclk(clk), .aresetn(resetn), .s_axis_tdata(data), .s_axis_tvalid(valid),
        .s_axis_tready(in_ready), .s_axis_tlast(last), .m_axis_tdata(sum),
        .m_axis_tvalid(out_valid), .m_axis_tready(ready), .m_axis_tlast(out_last),
        .m_axis_tuser(bin), .frame_index(frame), .frame_error(error),
        .result_count(result_count)
    );
    integer ticks = 0, received = 0;
    integer completed_expected = 0;
    integer expected_group = 0, expected_bin = 0;
    reg check_output = 0;
    reg was_stalled = 0;
    reg [31:0] held_data, held_bin;
    reg held_last;
    shortreal expected;
    reg [31:0] expected_bits;
    always @(negedge clk) begin
        ticks = ticks + 1;
        ready = !force_stall && (!STALLS || (ticks > 200 && ticks % 17 < 9));
    end
    always @(posedge clk) begin
        if (!resetn) begin was_stalled = 0; completed_expected = 0; end
        else begin
            if (result_count !== completed_expected)
                $fatal(1, "Completion count changed before downstream last handshake");
            if (was_stalled && (!out_valid || sum !== held_data || bin !== held_bin || out_last !== held_last))
                $fatal(1, "AXIS output changed under backpressure");
            was_stalled = out_valid && !ready;
            held_data = sum; held_bin = bin; held_last = out_last;
            if (out_valid && ready) begin
                if (out_last) completed_expected = completed_expected + 1;
                if (!check_output) $fatal(1, "Discarded/reset group produced output");
                expected = N_FRAMES*(expected_bin+1.25+100*expected_group) + 10*N_FRAMES*(N_FRAMES+1)/2;
                if (expected_group % 2) expected = -expected;
                expected_bits = $shortrealtobits(expected);
                if (sum !== expected_bits || bin !== expected_bin || out_last !== (expected_bin == FRAME_LENGTH-1))
                    $fatal(1, "group=%0d bin=%0d got=%h expected=%h user=%0d last=%b",
                           expected_group, expected_bin, sum, expected_bits, bin, out_last);
                received = received + 1;
                if (expected_bin == FRAME_LENGTH-1) begin
                    expected_bin = 0; expected_group = expected_group + 1;
                end else expected_bin = expected_bin + 1;
            end
        end
    end
    task beat(input shortreal value, input bit final_bin);
        begin
            @(negedge clk);
            data = $shortrealtobits(value); valid = 1; last = final_bin;
            @(posedge clk);
            while (!in_ready) @(posedge clk);
        end
    endtask
    task idle;
        begin @(negedge clk); valid = 0; last = 0; data = 32'h7fc00000; end
    endtask
    integer g, f, b;
    initial begin
        repeat (8) @(negedge clk);
        resetn = 1;
        // Reset a partial group, including arithmetic already in flight.
        beat(500, FRAME_LENGTH == 1);
        idle(); resetn = 0;
        repeat (8) @(negedge clk);
        resetn = 1;
        // Reset a completed result while the receiver holds it stalled.
        for (f=0; f<N_FRAMES; f=f+1)
            for (b=0; b<FRAME_LENGTH; b=b+1)
                beat(700, !CHECK_TLAST || b == FRAME_LENGTH-1);
        idle();
        repeat (20) @(negedge clk);
        if (!out_valid) $fatal(1, "Completed result did not become valid");
        resetn = 0;
        repeat (8) @(negedge clk);
        resetn = 1; force_stall = 0;
        repeat (3) @(negedge clk);
        if (out_valid) $fatal(1, "Reset retained a stalled output");
        if (CHECK_TLAST) begin
        // A late TLAST invalidates the group, and the next TLAST resynchronizes.
        for (b=0; b<FRAME_LENGTH; b=b+1) beat(900, 0);
        beat(900, 1);
        idle();
        repeat (20) @(negedge clk);
        if (!error) $fatal(1, "Missing framing error");
        if (FRAME_LENGTH > 1) begin
            beat(800, 1); // early TLAST
            idle();
            repeat (20) @(negedge clk);
        end
        end
        check_output = 1;
        for (g=0; g<6; g=g+1)
            for (f=0; f<N_FRAMES; f=f+1)
                for (b=0; b<FRAME_LENGTH; b=b+1) begin
                    beat((g % 2 ? -1 : 1)*(b+1.25+100*g+10*(f+1)), CHECK_TLAST && b == FRAME_LENGTH-1);
                    if (STALLS && (b+f) % 7 == 0) begin
                        idle(); repeat (3) @(negedge clk);
                    end
                end
        idle();
        wait (received == 6*FRAME_LENGTH);
        repeat (30) @(negedge clk);
        if (out_valid) $fatal(1, "Unexpected additional result");
        // Reset after a completed run clears the sticky error.
        resetn = 0;
        repeat (3) @(negedge clk);
        resetn = 1;
        repeat (20) @(negedge clk);
        if (error || out_valid) $fatal(1, "Reset did not clear state");
        passed = 1;
        $display("PASS: bins=%0d frames=%0d stalls=%0d TLAST=%0d", FRAME_LENGTH, N_FRAMES, STALLS, CHECK_TLAST);
        $finish;
    end
    initial begin #1000000; $fatal(1, "Timeout"); end
endmodule
