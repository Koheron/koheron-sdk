`timescale 1 ns / 1 ps

module test_detector_tb;
  reg aclk = 0, aresetn = 0, acc_on = 0, valid = 0;
  reg [31:0] data_a = 0, data_b = 0;
  wire [31:0] phase_old, phase_new;
  wire [16:0] freq_old, freq_new;
  wire valid_old, valid_new;
  reg [31:0] phase_d1, phase_d2;
  reg [16:0] freq_d1, freq_d2;
  integer cycle = 0, checked = 0, i, seed = 91473;

  always #2.5 aclk = ~aclk;
  detector_wrapper dut (
    .aclk(aclk), .aresetn(aresetn), .acc_on(acc_on), .valid(valid),
    .data_a(data_a), .data_b(data_b),
    .phase_old(phase_old), .phase_new(phase_new),
    .freq_old(freq_old), .freq_new(freq_new),
    .valid_old(valid_old), .valid_new(valid_new)
  );

  always @(posedge aclk) begin
    phase_d1 <= phase_new;
    phase_d2 <= phase_d1;
    freq_d1 <= freq_new;
    freq_d2 <= freq_d1;
    cycle = cycle + 1;
    #0.1;
    if (cycle > 260) begin
      if ((^{phase_old, phase_new, freq_old, freq_new}) === 1'bx)
        $fatal(1, "Unknown detector output at cycle %0d", cycle);
      if (valid_old !== 1'b1 || valid_new !== 1'b1)
        $fatal(1, "Detector output throughput changed at cycle %0d", cycle);
      if (phase_old !== phase_d2 || freq_old !== freq_d2)
        $fatal(1, "Detector mismatch at cycle %0d: phase %h/%h, frequency %h/%h",
               cycle, phase_old, phase_d2, freq_old, freq_d2);
      checked = checked + 1;
    end
  end

  initial begin
    repeat (10) @(negedge aclk);
    aresetn = 1;
    valid = 1;
    // Start accumulation only after both pipelines have settled at zero.
    repeat (200) @(negedge aclk);
    acc_on = 1;
    repeat (64) @(negedge aclk);
    for (i = 0; i < 8192; i = i + 1) begin
      @(negedge aclk);
      valid = (i % 19 != 18);
      data_b = $random(seed);
      if (i < 128) data_a = (i % 2) ? 32'h00007fff : 32'h00008000;
      else if (i < 256) data_a = (i % 4 == 0) ? 32'h00000001 : 0;
      else if (i < 384) data_a = (i % 4 == 0) ? 32'h0000ffff : 0;
      else data_a = $random(seed) & 32'h0000ffff;
    end
    valid = 1;
    data_a = 0;
    data_b = 0;
    repeat (100) @(negedge aclk);
    $display("Detector checks passed: %0d samples, identical phase/frequency with two cycles less delay", checked);
    $finish;
  end
endmodule
