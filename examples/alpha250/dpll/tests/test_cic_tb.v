`timescale 1 ns / 1 ps
module test_cic_tb;
    localparam N = 65536;
    reg clk = 0, vin = 0, cfg_valid = 0;
    reg [31:0] din = 0;
    wire [2:0] ready_old, ready_new, cfg_old, cfg_new, valid_old, valid_new;
    wire [31:0] dout_old [0:2], dout_new [0:2];
    reg [31:0] samples_old [0:2][0:N/4], samples_new [0:2][0:N/4];
    integer count_old [0:2], count_new [0:2];
    reg [2:0] configured_old = 0, configured_new = 0;
    integer i, j, n, expected, checked = 0, seed = 35621;
    always #2 clk = ~clk;
    cic_test_wrapper dut (
        .clk(clk), .vin(vin), .din(din), .cfg_valid(cfg_valid),
        .ready_old_4(ready_old[0]),
        .cfg_ready_old_4(cfg_old[0]),
        .valid_old_4(valid_old[0]),
        .dout_old_4(dout_old[0]),
        .ready_new_4(ready_new[0]),
        .cfg_ready_new_4(cfg_new[0]),
        .valid_new_4(valid_new[0]),
        .dout_new_4(dout_new[0]),
        .ready_old_20(ready_old[1]),
        .cfg_ready_old_20(cfg_old[1]),
        .valid_old_20(valid_old[1]),
        .dout_old_20(dout_old[1]),
        .ready_new_20(ready_new[1]),
        .cfg_ready_new_20(cfg_new[1]),
        .valid_new_20(valid_new[1]),
        .dout_new_20(dout_new[1]),
        .ready_old_8192(ready_old[2]),
        .cfg_ready_old_8192(cfg_old[2]),
        .valid_old_8192(valid_old[2]),
        .dout_old_8192(dout_old[2]),
        .ready_new_8192(ready_new[2]),
        .cfg_ready_new_8192(cfg_new[2]),
        .valid_new_8192(valid_new[2]),
        .dout_new_8192(dout_new[2])
    );
    always @(posedge clk) begin
        for (j = 0; j < 3; j = j + 1) begin
            if (cfg_valid && cfg_old[j]) configured_old[j] = 1;
            if (cfg_valid && cfg_new[j]) configured_new[j] = 1;
            if (vin && (ready_old[j] !== 1 || ready_new[j] !== 1))
                $fatal(1, "CIC input stalled at rate index %0d", j);
            if (valid_old[j]) begin
                if ((^dout_old[j]) === 1'bx || count_old[j] >= N/4)
                    $fatal(1, "Invalid reference CIC output");
                samples_old[j][count_old[j]] = dout_old[j];
                count_old[j] = count_old[j] + 1;
            end
            if (valid_new[j]) begin
                if ((^dout_new[j]) === 1'bx || count_new[j] >= N/4)
                    $fatal(1, "Invalid DSP CIC output");
                samples_new[j][count_new[j]] = dout_new[j];
                count_new[j] = count_new[j] + 1;
            end
        end
    end
    initial begin
        for (i = 0; i < 3; i = i + 1) begin count_old[i] = 0; count_new[i] = 0; end
        repeat (64) @(negedge clk);
        cfg_valid = 1;
        repeat (32) @(negedge clk);
        cfg_valid = 0;
        repeat (64) @(negedge clk);
        if (configured_old !== 3'b111 || configured_new !== 3'b111)
            $fatal(1, "CIC rate configuration was not accepted");
        for (n = 0; n < N; n = n + 1) begin
            @(negedge clk);
            vin = 1;
            if (n < 64) din = (n == 0) ? 32'h7fffffff : 0;
            else if (n < 128) din = (n % 2) ? 32'h7fffffff : 32'h80000000;
            else din = $random(seed);
        end
        @(negedge clk); vin = 0;
        repeat (256) @(negedge clk);
        for (i = 0; i < 3; i = i + 1) begin
            expected = (N + ((i == 0) ? 4 : ((i == 1) ? 20 : 8192)) - 1) /
                       ((i == 0) ? 4 : ((i == 1) ? 20 : 8192));
            if (count_old[i] != expected || count_new[i] != expected)
                $fatal(1, "CIC sample count mismatch at rate index %0d: %0d/%0d", i, count_old[i], count_new[i]);
            for (n = 0; n < count_old[i]; n = n + 1) begin
                if (samples_old[i][n] !== samples_new[i][n])
                    $fatal(1, "CIC arithmetic mismatch rate index=%0d sample=%0d old=%h new=%h", i, n, samples_old[i][n], samples_new[i][n]);
                checked = checked + 1;
            end
        end
        $display("CIC checks passed: %0d exact outputs, rates 4/20/8192, sustained 250 MHz input", checked);
        $finish;
    end
endmodule
