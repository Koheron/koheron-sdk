`timescale 1ns/1ps
module quad_capture_tb;
    reg clk = 0;
    always #2.5 clk = ~clk;
    reg aresetn = 0, trigger = 0, test_pattern = 0;
    reg [31:0] sample_count = 22;
    reg [15:0] adc00 = 0, adc01 = 0, adc10 = 0, adc11 = 0;
    wire [31:0] status, captured_samples;
    wire [63:0] data0, data1;
    wire valid0, valid1, last0, last1;
    reg ready0 = 1, ready1 = 1;
    wire [7:0] keep0, keep1;
    reg passed = 0;
    integer source_index = 0, start_index = 0, received0 = 0, received1 = 0;
    integer cycles = 0;
    reg check_data = 0, check_drain = 0, pattern_expected = 0, held0 = 0, held1 = 0;
    reg [63:0] held_data0, held_data1;
    reg held_last0, held_last1;
    quad_capture #(.PACKET_BEATS(4)) dut (
        .clk(clk), .aresetn(aresetn), .trigger(trigger), .sample_count(sample_count),
        .test_pattern(test_pattern), .adc00(adc00), .adc01(adc01), .adc10(adc10), .adc11(adc11),
        .status(status), .captured_samples(captured_samples),
        .m0_axis_tdata(data0), .m0_axis_tvalid(valid0), .m0_axis_tready(ready0),
        .m0_axis_tlast(last0), .m0_axis_tkeep(keep0),
        .m1_axis_tdata(data1), .m1_axis_tvalid(valid1), .m1_axis_tready(ready1),
        .m1_axis_tlast(last1), .m1_axis_tkeep(keep1)
    );

    // Drive different physical codes every ADC clock, independent of TREADY.
    always @(negedge clk) begin
        source_index = source_index + 1;
        adc00 = source_index;
        adc01 = source_index ^ 16'h4000;
        adc10 = source_index ^ 16'h8000;
        adc11 = source_index ^ 16'hc000;
    end
    function automatic [63:0] expected(input integer pair, input integer frame);
        reg [15:0] a, b, high_a, high_b;
        begin
            a = pattern_expected ? frame : start_index + frame;
            b = a + 1'b1;
            high_a = pattern_expected ? (frame >> 16) : a;
            high_b = pattern_expected ? ((frame + 1) >> 16) : b;
            expected = {high_b ^ (pair ? 16'hc000 : 16'h4000), b ^ (pair ? 16'h8000 : 16'h0000),
                        high_a ^ (pair ? 16'hc000 : 16'h4000), a ^ (pair ? 16'h8000 : 16'h0000)};
        end
    endfunction
    always @(posedge clk) begin
        cycles = cycles + 1;
        if (aresetn) begin
            if (held0 && (!valid0 || data0 !== held_data0 || last0 !== held_last0)) $fatal(1, "AXIS0 changed while stalled");
            if (held1 && (!valid1 || data1 !== held_data1 || last1 !== held_last1)) $fatal(1, "AXIS1 changed while stalled");
            if ((check_data || check_drain) && valid0 && ready0) begin
                if (check_data && (data0 !== expected(0, 2 * received0) || keep0 != 8'hff)) $fatal(1, "Pair 0 sample order at beat %d", received0);
                if (last0 !== ((received0 % 4 == 3) || (received0 == sample_count / 2 - 1))) $fatal(1, "Pair 0 packet boundary");
                received0 = received0 + 1;
            end
            if ((check_data || check_drain) && valid1 && ready1) begin
                if (check_data && (data1 !== expected(1, 2 * received1) || keep1 != 8'hff)) $fatal(1, "Pair 1 sample order at beat %d", received1);
                if (last1 !== ((received1 % 4 == 3) || (received1 == sample_count / 2 - 1))) $fatal(1, "Pair 1 packet boundary");
                received1 = received1 + 1;
            end
        end
        held0 = aresetn && valid0 && !ready0;
        held1 = aresetn && valid1 && !ready1;
        held_data0 = data0; held_data1 = data1;
        held_last0 = last0; held_last1 = last1;
        if (cycles > 150000) $fatal(1, "Simulation timeout");
    end

    task reset_capture;
        begin
            @(negedge clk); #0.1;
            aresetn = 0; trigger = 0; ready0 = 1; ready1 = 1; check_data = 0; check_drain = 0;
            repeat (4) @(negedge clk);
            #0.1; received0 = 0; received1 = 0; aresetn = 1;
        end
    endtask
    task start_capture(input integer samples, input bit pattern_mode);
        begin
            reset_capture();
            sample_count = samples; test_pattern = pattern_mode; pattern_expected = pattern_mode;
            @(negedge clk); #0.1;
            start_index = source_index;
            check_data = 1; trigger = 1;
            @(negedge clk); #0.1; trigger = 0;
        end
    endtask
    task finish_capture;
        begin
            wait(status[1]);
            wait(received0 == sample_count / 2 && received1 == sample_count / 2);
            repeat (3) @(negedge clk);
            #0.1;
            if (status !== 32'd2 || captured_samples != sample_count) $fatal(1, "Wrong completion or sample count");
        end
    endtask

    initial begin
        // Analog codes, multiple packets, short final packet, unequal one-cycle stalls.
        start_capture(22, 0);
        repeat (5) begin
            @(negedge clk); #0.1; ready0 = 0;
            @(negedge clk); #0.1; ready0 = 1; ready1 = 0;
            @(negedge clk); #0.1; ready1 = 1;
        end
        finish_capture();
        // Full packet and minimum record, repeating without stale state.
        start_capture(8, 0); finish_capture();
        start_capture(2, 0); finish_capture();
        // Internal counter rollover must not alter channel/sample ordering.
        start_capture(65542, 1); finish_capture();
        // Overflow preserves pending beats and drains every packet on BOTH paths.
        for (integer pair = 0; pair < 3; pair = pair + 1) begin
            start_capture(pair == 1 ? 6 : 32, 0);
            check_data = 0; check_drain = 1;
            @(negedge clk); #0.1;
            ready0 = pair == 1; ready1 = pair == 0;
            wait(status[2] || status[3]);
            repeat (4) @(negedge clk);
            #0.1;
            if (status[1:0] != 0 || !status[5] || status[3:2] != (pair == 2 ? 3 : (pair ? 2 : 1))) $fatal(1, "Incorrect overflow flags");
            ready0 = 1; ready1 = 1;
            wait(status[1] && received0 == sample_count / 2 && received1 == sample_count / 2);
            @(negedge clk); #0.1;
            if (status[5] || !status[3:2] || captured_samples >= sample_count) $fatal(1, "Overflow did not drain with sticky error");
        end
        // Odd, zero and oversized records must never start.
        for (integer mode = 0; mode < 3; mode = mode + 1) begin
            reset_capture();
            sample_count = mode == 0 ? 3 : (mode == 1 ? 0 : 16777218);
            trigger = 1;
            repeat (3) @(negedge clk);
            #0.1;
            if (status !== 32'd16 || captured_samples != 0) $fatal(1, "Invalid length accepted");
        end
        passed = 1;
        $display("PASS: shared trigger, analog ordering, packets, repeat capture, pattern rollover, drained overflows and invalid lengths");
        $finish;
    end
endmodule
