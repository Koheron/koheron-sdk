`timescale 1 ns / 1 ps
module test_gain_tb;
    reg clk = 0;
    reg signed [47:0] a = 0;
    reg signed [31:0] b = 0;
    wire [31:0] p, pi, i2;
    wire [63:0] i3;
    reg signed [79:0] product48;
    reg signed [63:0] product32;
    reg signed [48:0] product17;
    reg [31:0] ref_p [0:2], ref_pi [0:2], ref_i2 [0:2];
    reg [63:0] ref_i3 [0:2];
    integer cycle = 0, checked = 0, n, j, seed = 79631;
    always #2 clk = ~clk;
    gain_multiplier #(.A_WIDTH(17), .OUTPUT_WIDTH(32)) gp(clk, a[16:0], b, p);
    gain_multiplier #(.OUTPUT_LOW(16), .OUTPUT_WIDTH(32)) gpi(clk, a[31:0], b, pi);
    gain_multiplier #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32)) gi2(clk, a, b, i2);
    gain_multiplier gi3(clk, a[31:0], b, i3);
    always @(posedge clk) begin
        product17 = $signed(a[16:0]) * b;
        product32 = $signed(a[31:0]) * b;
        product48 = a * b;
        ref_p[0] <= product17[31:0];
        ref_pi[0] <= product32[47:16];
        ref_i2[0] <= product48[79:48];
        ref_i3[0] <= product32;
        for (j = 1; j < 3; j = j + 1) begin
            ref_p[j] <= ref_p[j-1]; ref_pi[j] <= ref_pi[j-1];
            ref_i2[j] <= ref_i2[j-1]; ref_i3[j] <= ref_i3[j-1];
        end
        cycle = cycle + 1;
        #1;
        if (cycle > 32) begin
            if ({p, pi, i2, i3} !== {ref_p[2], ref_pi[2], ref_i2[2], ref_i3[2]})
                $fatal(1, "Gain product/latency mismatch at cycle %0d", cycle);
            checked = checked + 1;
        end
    end
    initial begin
        repeat (32) @(negedge clk);
        for (n = 0; n < 10000; n = n + 1) begin
            @(negedge clk);
            if (n < 32) begin
                a = (n % 2) ? 48'h7fffffffffff : 48'h800000000000;
                b = (n % 4 < 2) ? 32'h7fffffff : 32'h80000000;
            end else if (n < 64) begin
                a = (n % 2) ? 48'h00007fffffff : 48'hffff80000000;
                b = (n % 4 < 2) ? 32'h7fffffff : 32'h80000000;
            end else if (n < 96) begin
                a = (n % 2) ? 48'h00000000ffff : 48'hffffffff0000;
                b = (n % 4 < 2) ? 32'h7fffffff : 32'h80000000;
            end else if (n < 128) begin
                a = (n % 2) ? 1 : -1; b = (n % 4 < 2) ? 1 : -1;
            end else begin
                a = {$random(seed), $random(seed)}; b = $random(seed);
            end
        end
        repeat (4) @(negedge clk);
        $display("Gain checks passed: %0d cycles, full 17/32/48-bit signed ranges, exact three-clock latency", checked);
        $finish;
    end
endmodule
