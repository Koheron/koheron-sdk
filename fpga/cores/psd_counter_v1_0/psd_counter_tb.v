`timescale 1 ns / 1 ps

// Tagged samples make a one-bin shift visible; constant data cannot detect it.
module psd_counter_case #(
  parameter PERIOD = 64,
  parameter N_CYCLES = 3,
  parameter GAPS = 0,
  parameter PERIOD_WIDTH = (PERIOD > 1) ? $clog2(PERIOD) : 1,
  parameter N_CYCLES_WIDTH = (N_CYCLES > 1) ? $clog2(N_CYCLES) : 1
)(output reg done = 0);
  reg clk = 0;
  always #2 clk = ~clk;
  reg valid = 0;
  reg [31:0] data = 0;
  wire out_valid, first_cycle, last_cycle;
  wire [31:0] out_data;
  wire [PERIOD_WIDTH+1:0] addr;
  wire [N_CYCLES_WIDTH-1:0] cycle_index;

  psd_counter #(
    .PERIOD(PERIOD), .PERIOD_WIDTH(PERIOD_WIDTH),
    .N_CYCLES(N_CYCLES), .N_CYCLES_WIDTH(N_CYCLES_WIDTH)
  ) dut (
    .clk(clk), .s_axis_tvalid(valid), .s_axis_tdata(data),
    .m_axis_tvalid(out_valid), .m_axis_tdata(out_data), .addr(addr),
    .cycle_index(cycle_index), .first_cycle(first_cycle), .last_cycle(last_cycle)
  );

  integer sent = 0;
  integer received = 0;
  integer tick = 0;
  integer expected_bin = 0;
  integer expected_cycle = 0;
  reg expected_valid = 0;
  reg [31:0] expected_data = 0;

  // Check the tuple seen by a downstream synchronous consumer, before NBA updates.
  always @(posedge clk) begin
    if (out_valid !== expected_valid)
      $fatal(1, "valid latency: period=%0d cycles=%0d", PERIOD, N_CYCLES);
    if (expected_valid) begin
      if (out_data !== expected_data || addr !== (expected_bin * 4) ||
          cycle_index !== expected_cycle ||
          first_cycle !== (expected_cycle == 0) ||
          last_cycle !== (expected_cycle == N_CYCLES - 1))
        $fatal(1, "tuple mismatch: period=%0d cycles=%0d data=%0d/%0d bin=%0d/%0d cycle=%0d/%0d",
               PERIOD, N_CYCLES, out_data, expected_data, addr/4,
               expected_bin, cycle_index, expected_cycle);
      received = received + 1;
    end else if (tick > 1 && (first_cycle !== 0 || last_cycle !== 0)) begin
      $fatal(1, "invalid sample must not clear or publish an accumulator bin");
    end
    expected_valid = valid;
    if (valid) begin
      expected_data = data;
      expected_bin = sent % PERIOD;
      expected_cycle = (sent / PERIOD) % N_CYCLES;
      sent = sent + 1;
    end
    tick = tick + 1;
  end

  initial begin
    repeat (4) @(negedge clk);
    while (sent < 2 * PERIOD * N_CYCLES) begin
      valid = !GAPS || ((tick % 7 != 0) && (tick % 11 < 9));
      data = valid ? sent + 1 : 32'hdeadbeef;
      @(negedge clk);
    end
    valid = 0;
    repeat (4) @(negedge clk);
    if (received != sent) $fatal(1, "lost or duplicated sample");
    $display("PASS period=%0d cycles=%0d gaps=%0d samples=%0d", PERIOD, N_CYCLES, GAPS, received);
    done = 1;
  end
endmodule

module psd_counter_tb;
  wire [5:0] done;
  psd_counter_case #(.PERIOD(64), .N_CYCLES(8)) c0(done[0]);
  psd_counter_case #(.PERIOD(5), .N_CYCLES(3), .GAPS(1)) c1(done[1]);
  psd_counter_case #(.PERIOD(1), .N_CYCLES(3), .GAPS(1)) c2(done[2]);
  psd_counter_case #(.PERIOD(1), .N_CYCLES(1)) c3(done[3]);
  psd_counter_case #(.PERIOD(8192), .N_CYCLES(3)) c4(done[4]);
  psd_counter_case #(.PERIOD(16), .N_CYCLES(1023), .GAPS(1)) c5(done[5]);
  initial begin
    wait (&done);
    $display("PASS: all PSD counter cases");
    $finish;
  end
  initial begin
    #1000000;
    $fatal(1, "timeout");
  end
endmodule
