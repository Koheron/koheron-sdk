`timescale 1 ns / 1 ps

module test_detector_tb;
  reg aclk = 0, aresetn = 0, acc_on = 0, valid = 0;
  reg [31:0] data_a = 0, data_b = 0;
  wire [31:0] phase_old, phase_new;
  wire [16:0] freq_old, freq_new;
  wire valid_old, valid_new;
  wire [31:0] phase_fast;
  wire [16:0] freq_fast;
  wire valid_fast;
  wire signed [39:0] phase_precise;
  wire signed [24:0] freq_precise;
  wire signed [23:0] iq_i_fast, iq_q_fast;
  reg [31:0] fast_phase_delay [0:6];
  reg [16:0] fast_freq_delay [0:6];
  reg [31:0] phase_d1, phase_d2;
  reg [16:0] freq_d1, freq_d2;
  integer cycle = 0, checked = 0, i, seed = 91473;
  integer k, fast_checked = 0, phase_offset = 0, phase_error, freq_error;
  integer fractional_samples = 0, fine_phase_samples = 0;
  reg check_fast = 0, offset_set = 0;
  real carrier, modulation;
  integer adc_sample, ref_i, ref_q;

  always #2 aclk = ~aclk;
  detector_wrapper dut (
    .aclk(aclk), .aresetn(aresetn), .acc_on(acc_on), .valid(valid),
    .data_a(data_a), .data_b(data_b),
    .phase_old(phase_old), .phase_new(phase_new),
    .freq_old(freq_old), .freq_new(freq_new),
    .valid_old(valid_old), .valid_new(valid_new),
    .phase_fast(phase_fast), .freq_fast(freq_fast), .valid_fast(valid_fast),
    .iq_i_fast(iq_i_fast), .iq_q_fast(iq_q_fast),
    .phase_precise(phase_precise), .freq_precise(freq_precise)
  );

  always @(posedge aclk) begin
    phase_d1 <= phase_new;
    phase_d2 <= phase_d1;
    freq_d1 <= freq_new;
    freq_d2 <= freq_d1;
    fast_phase_delay[0] <= phase_fast;
    fast_freq_delay[0] <= freq_fast;
    for (k=1;k<7;k=k+1) begin
      fast_phase_delay[k] <= fast_phase_delay[k-1];
      fast_freq_delay[k] <= fast_freq_delay[k-1];
    end
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
      if (check_fast) begin
        if ((^{iq_i_fast,iq_q_fast}) === 1'bx) $fatal(1,"Unknown 24-bit IQ");
        if (iq_i_fast[7:0] != 0 || iq_q_fast[7:0] != 0)
          fractional_samples = fractional_samples + 1;
        if (valid_fast !== 1'b1 || (^{phase_fast, freq_fast}) === 1'bx)
          $fatal(1,"Invalid fast detector output");
        if (phase_fast !== phase_precise[39:8] || freq_fast !== freq_precise[24:8])
          $fatal(1,"Legacy phase/frequency scale changed");
        if (phase_precise[7:0] != 0 || freq_precise[7:0] != 0) fine_phase_samples = fine_phase_samples+1;
        phase_error = $signed(phase_old) - $signed(fast_phase_delay[6]);
        if (!offset_set) begin phase_offset=phase_error; offset_set=1; end
        phase_error = phase_error - phase_offset;
        freq_error = $signed(freq_old) - $signed(fast_freq_delay[6]);
        if (phase_error > 4 || phase_error < -4 || freq_error > 4 || freq_error < -4)
          $fatal(1,"Fast detector phase/frequency mismatch at cycle %0d: phase=%0d frequency=%0d",cycle,phase_error,freq_error);
        fast_checked = fast_checked + 1;
      end
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
    // Real ADC carrier plus complex DDS reference. Carrier fs/8 makes the
    // unwanted twice-carrier component fall at the boxcar's first zero.
    // Slowly changing residual phase traverses many full turns in both
    // directions, checking orientation, latency, filtering and unwrapping.
    for (i=0;i<16384;i=i+1) begin
      carrier = 6.283185307179586 * i / 8.0;
      modulation = 0.02 * i + 3.0 * $sin(i / 80.0);
      adc_sample = $rtoi(28000.0 * $cos(carrier + modulation));
      ref_i = $rtoi(30000.0 * $cos(carrier));
      ref_q = $rtoi(-30000.0 * $sin(carrier));
      data_a = {16'b0,adc_sample[15:0]};
      data_b = {ref_q[15:0],ref_i[15:0]};
      if (i == 300) check_fast=1;
      @(negedge aclk);
    end
    $display("Detector checks passed: %0d samples, identical phase/frequency with two cycles less delay", checked);
    if(fast_checked<16000) $fatal(1,"Too few fast detector checks");
    if(fractional_samples<8000) $fatal(1,"Fractional Cartesian bits were discarded");
    if(fine_phase_samples<8000) $fatal(1,"Fractional phase bits were discarded");
    $display("Fine phase feedback checks passed: %0d samples retain fractional phase/frequency bits",fine_phase_samples);
    $display("Fast detector checks passed: %0d samples, quadrant crossings and phase/frequency aligned seven clocks before the original detector; %0d samples retain fractional IQ bits",fast_checked,fractional_samples);
    $finish;
  end
endmodule
