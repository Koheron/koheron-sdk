`timescale 1ns / 1ps
module psd_frame_tb;
    reg passed = 0, clk = 0, valid = 0, last = 0;
    always #2 clk = !clk;
    reg [63:0] data = 0;
    wire ready, out_valid, out_last;
    wire [31:0] power;
    system_wrapper dut(.clk(clk), .data(data), .valid(valid), .last(last),
        .ready(ready), .power(power), .out_valid(out_valid), .out_last(out_last));
    integer sent, received = 0;
    shortreal a, b, expected;
    reg [31:0] expected_bits;
    always @(posedge clk) begin
        if (out_valid) begin
            expected = 5*(received+1)*(received+1);
            expected_bits = $shortrealtobits(expected);
            if (power !== expected_bits || out_last !== (received % 16 == 15))
                $fatal(1, "PSD data/TLAST alignment failed at sample %0d", received);
            received = received + 1;
        end
    end
    initial begin
        repeat (12) @(negedge clk);
        for (sent=0; sent<128; sent=sent+1) begin
            @(negedge clk);
            valid = 1; last = sent % 16 == 15;
            a = sent+1; b = 2*(sent+1);
            data = {$shortrealtobits(b), $shortrealtobits(a)};
            @(posedge clk);
            while (!ready) @(posedge clk);
            if (sent % 7 == 0) begin
                @(negedge clk); valid = 0; last = 0;
                data = 64'h7fc000007fc00000;
                repeat (3) @(negedge clk);
            end
        end
        @(negedge clk); valid = 0; last = 0;
        repeat (200) @(negedge clk);
        if (received != 128) $fatal(1, "Missing PSD output samples: %0d", received);
        passed = 1;
        $display("PASS: vendor PSD arithmetic preserves TLAST/data through pauses");
        $finish;
    end
    initial begin #100000; $fatal(1, "Timeout"); end
endmodule
