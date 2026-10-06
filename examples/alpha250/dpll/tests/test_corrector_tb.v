`timescale 1 ns / 1 ps

module test_corrector_tb;
  reg clk = 0;
  reg signed [31:0] phase_in = 0, p_gain = 0, pi_gain = 0, i2_gain = 0, i3_gain = 0;
  reg signed [16:0] freq_in = 0;
  reg [2:0] sclr = 0;
  wire [15:0] fast_old, fast_new, slow_old, slow_new;
  wire [31:0] p_old, p_new, pi_old, pi_new, i2_old, i2_new, acc2_old, acc2_new;
  wire [63:0] i3_old, i3_new;
  wire [47:0] acc1_old, acc1_new;
  reg signed [63:0] product_p, product_pi, product_i3;
  reg signed [79:0] product_i2;
  reg [31:0] ref_p [0:2], ref_pi [0:2], ref_i2 [0:2];
  reg [63:0] ref_i3 [0:2];
  integer cycle = 0, checked = 0, i, j, seed = 62719;

  always #2 clk = ~clk;
  corrector_wrapper dut (
    .clk(clk), .phase_in(phase_in), .freq_in(freq_in), .sclr(sclr),
    .p_gain(p_gain), .pi_gain(pi_gain), .i2_gain(i2_gain), .i3_gain(i3_gain),
    .fast_old(fast_old), .fast_new(fast_new), .slow_old(slow_old), .slow_new(slow_new),
    .p_old(p_old), .p_new(p_new), .pi_old(pi_old), .pi_new(pi_new),
    .i2_old(i2_old), .i2_new(i2_new), .i3_old(i3_old), .i3_new(i3_new),
    .acc1_old(acc1_old), .acc1_new(acc1_new), .acc2_old(acc2_old), .acc2_new(acc2_new)
  );

  always @(posedge clk) begin
    // Independent full-width signed products and the existing output slices.
    product_p = $signed(freq_in) * $signed(p_gain);
    product_pi = $signed(phase_in) * $signed(pi_gain);
    product_i2 = $signed(acc1_new) * $signed(i2_gain);
    product_i3 = $signed(acc2_new) * $signed(i3_gain);
    ref_p[0] <= product_p[31:0];
    ref_pi[0] <= product_pi[47:16];
    ref_i2[0] <= product_i2[79:48];
    ref_i3[0] <= product_i3;
    for (j = 1; j < 3; j = j + 1) begin
      ref_p[j] <= ref_p[j-1];
      ref_pi[j] <= ref_pi[j-1];
      ref_i2[j] <= ref_i2[j-1];
      ref_i3[j] <= ref_i3[j-1];
    end
    cycle = cycle + 1;
    // DSP primitive models have propagation delays after the clock edge.
    #1;
    if (cycle > 32) begin
      if ((^{fast_new, slow_new, p_new, pi_new, i2_new, i3_new, acc1_new, acc2_new}) === 1'bx)
        $fatal(1, "Unknown corrector output at cycle %0d", cycle);
      if ({fast_old, slow_old, p_old, pi_old, i2_old, i3_old, acc1_old, acc2_old} !==
          {fast_new, slow_new, p_new, pi_new, i2_new, i3_new, acc1_new, acc2_new})
        $fatal(1, "LUT/DSP mismatch cycle=%0d p=%h/%h pi=%h/%h i2=%h/%h i3=%h/%h acc1=%h/%h acc2=%h/%h refs=%h/%h/%h/%h",
               cycle, p_old, p_new, pi_old, pi_new, i2_old, i2_new, i3_old, i3_new,
               acc1_old, acc1_new, acc2_old, acc2_new, ref_p[2], ref_pi[2], ref_i2[2], ref_i3[2]);
      if ({p_new, pi_new, i2_new, i3_new} !== {ref_p[2], ref_pi[2], ref_i2[2], ref_i3[2]})
        $fatal(1, "Signed product or three-clock latency mismatch at cycle %0d", cycle);
      checked = checked + 1;
    end
  end

  initial begin
    repeat (40) @(negedge clk);
    for (i = 0; i < 10000; i = i + 1) begin
      @(negedge clk);
      sclr = (i % 97 == 0) ? 0 : ((i % 113 == 0) ? 3'b101 : 3'b111);
      if (i < 64) begin
        phase_in = (i % 2) ? 32'h7fffffff : 32'h80000000;
        freq_in = (i % 2) ? 17'h0ffff : 17'h10000;
        p_gain = (i % 4 < 2) ? 32'h7fffffff : 32'h80000000;
        pi_gain = p_gain;
        i2_gain = p_gain;
        i3_gain = p_gain;
      end else if (i < 128) begin
        phase_in = (i % 2) ? -1 : 1;
        freq_in = phase_in;
        p_gain = -1; pi_gain = 1; i2_gain = -1; i3_gain = 1;
      end else if (i < 160) begin
        phase_in = 0; freq_in = 0;
        p_gain = 0; pi_gain = 0; i2_gain = 0; i3_gain = 0;
      end else begin
        phase_in = $random(seed); freq_in = $random(seed);
        p_gain = $random(seed); pi_gain = $random(seed);
        i2_gain = $random(seed); i3_gain = $random(seed);
      end
    end
    repeat (20) @(negedge clk);
    $display("Corrector checks passed: %0d cycles, exact signed products, three-clock multipliers, unchanged controller latency", checked);
    $finish;
  end
endmodule
