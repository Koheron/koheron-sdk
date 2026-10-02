`timescale 1ns / 1ps
module legacy_tb;
    reg passed = 0;
    reg clk = 0;
    always #2 clk = !clk;
    reg resetn = 0, valid = 0, last = 0;
    reg [31:0] data = 0;
    wire [31:0] sum, addr, cycle;
    wire [3:0] wen;
    system_wrapper dut(.clk(clk), .resetn(resetn), .data(data), .valid(valid), .last(last),
                       .sum(sum), .addr(addr), .cycle(cycle), .wen(wen));
    integer sent, received = 0, bin, group_index, wraps = 0;
    reg [31:0] previous_cycle = 0;
    shortreal value, expected;
    reg [31:0] expected_bits;
    always @(posedge clk) begin
        if (wen == 4'hf) begin
            bin = received % 64; group_index = received / 64;
            expected = 3*(bin+1) + 100*(9*group_index+6);
            expected_bits = $shortrealtobits(expected);
            if (sum !== expected_bits || addr !== 4*bin)
                $fatal(1, "Legacy recorder mismatch output=%0d addr=%0d", received, addr);
            received = received + 1;
        end
        if (resetn && cycle < previous_cycle) begin
            if (received == 0 || received % 64 != 0)
                $fatal(1, "Progress wrapped before completed result write: %0d", received);
            wraps = wraps + 1;
        end
        previous_cycle = cycle;
    end
    initial begin
        repeat (8) @(negedge clk);
        resetn = 1;
        // The upstream FFT need not reset with the accumulator. Discard the
        // remainder of this partial frame and align to its real TLAST.
        for (sent=0; sent<12; sent=sent+1) begin
            valid = 1; data = 32'h7fc00000; last = sent == 11;
            @(negedge clk);
        end
        // Three result groups, continuous valid: the producer ignores TREADY.
        for (sent=0; sent<64*3*3; sent=sent+1) begin
            valid = 1; value = (sent%64)+1+100*((sent/64)+1);
            data = $shortrealtobits(value);
            last = sent % 64 == 63;
            @(negedge clk);
        end
        valid = 0; last = 0; data = 32'h7fc00000;
        repeat (200) @(negedge clk);
        if (received != 192 || wraps != 3)
            $fatal(1, "Missing legacy results/progress: bins=%0d wraps=%0d", received, wraps);
        // Reset only the accumulator midway through the next input frame.
        for (sent=0; sent<14; sent=sent+1) begin
            valid = 1; value = 9999; data = $shortrealtobits(value);
            @(negedge clk);
        end
        valid = 0; resetn = 0;
        repeat (8) @(negedge clk);
        resetn = 1;
        for (sent=14; sent<64; sent=sent+1) begin
            valid = 1; data = 32'h7fc00000; last = sent == 63;
            @(negedge clk);
        end
        for (sent=64*3*3; sent<64*3*6; sent=sent+1) begin
            valid = 1; value = (sent%64)+1+100*((sent/64)+1);
            data = $shortrealtobits(value); last = sent % 64 == 63;
            @(negedge clk);
        end
        valid = 0; last = 0;
        repeat (200) @(negedge clk);
        if (received != 384 || wraps != 6)
            $fatal(1, "Mid-frame reset lost alignment: bins=%0d wraps=%0d", received, wraps);
        passed = 1;
        $display("PASS: packaged adapter, TLAST reset alignment and complete BRAM write before progress wrap");
        $finish;
    end
    initial begin #100000; $fatal(1, "Timeout"); end
endmodule
