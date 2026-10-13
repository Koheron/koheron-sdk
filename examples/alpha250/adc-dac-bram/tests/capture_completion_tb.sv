`timescale 1ns/1ps
module capture_completion_tb;
  parameter integer COUNT_WIDTH = 5;
  parameter real PERIOD_NS = 4.0;
  localparam integer SAMPLES = 1 << COUNT_WIDTH;
  reg clk = 0, clken = 1, trig = 0;
  wire [31:0] address;
  wire [3:0] wen;
  wire capture_complete;
  integer writes = 0, completions = 0, frame_words = 0;
  reg previous_complete, final_write;

  address_counter #(.COUNT_WIDTH(COUNT_WIDTH)) dut (
    .clk(clk), .clken(clken), .trig(trig), .address(address),
    .wen(wen), .capture_complete(capture_complete)
  );
  always #(PERIOD_NS/2) clk = ~clk;

  // Model the actual BRAM write edge, then inspect the completion after NBA.
  always @(posedge clk) begin
    previous_complete = capture_complete;
    final_write = 0;
    if (wen != 0) begin
      if (!clken || wen != 4'hf || address != 4*frame_words)
        $fatal(1, "Wrong write address/enable: %d, expected %d", address, 4*frame_words);
      writes = writes + 1;
      frame_words = frame_words + 1;
      if (frame_words == SAMPLES) begin
        final_write = 1;
        frame_words = 0;
        completions = completions + 1;
      end
    end
    #1;
    if (capture_complete !== (previous_complete ^ final_write))
      $fatal(1, "Completion did not correspond to the final BRAM write");
  end

  task request_at(input integer offset);
    begin
      @(negedge clk);
      while (address != 4*offset) @(negedge clk);
      trig = 1;
      @(negedge clk); trig = 0;
    end
  endtask

  task wait_for_capture(input integer old_completions, input integer old_writes);
    integer cycles;
    begin
      cycles = 0;
      while (completions == old_completions && cycles < 2*SAMPLES+8) begin
        @(negedge clk); cycles = cycles + 1;
      end
      if (completions != old_completions+1 || writes != old_writes+SAMPLES)
        $fatal(1, "Capture missing or incomplete");
      repeat (SAMPLES+2) @(negedge clk);
      if (completions != old_completions+1 || writes != old_writes+SAMPLES)
        $fatal(1, "Capture repeated without another trigger");
    end
  endtask

  integer old_writes, old_completions;
  initial begin
    repeat (SAMPLES+2) @(negedge clk);
    if (writes != 0 || capture_complete != 0) $fatal(1, "Unexpected startup capture");
    old_writes=writes;old_completions=completions;
    request_at(0);wait_for_capture(old_completions,old_writes);
    old_writes=writes;old_completions=completions;
    request_at(SAMPLES/2);wait_for_capture(old_completions,old_writes);
    old_writes=writes;old_completions=completions;
    request_at(SAMPLES-1);wait_for_capture(old_completions,old_writes);
    // A pending request must survive a pause at the buffer boundary.
    old_writes=writes;old_completions=completions;
    request_at(SAMPLES-2);clken=0;
    repeat (5) @(negedge clk);
    if (writes != old_writes || completions != old_completions) $fatal(1, "Write during boundary pause");
    clken=1;wait_for_capture(old_completions,old_writes);
    // Pausing an active buffer must neither repeat addresses nor complete early.
    old_writes=writes;old_completions=completions;
    request_at(SAMPLES/2);
    while (writes < old_writes+3) @(negedge clk);
    clken=0;repeat (5) @(negedge clk);
    if (writes != old_writes+3 || completions != old_completions) $fatal(1, "Write during active pause");
    clken=1;wait_for_capture(old_completions,old_writes);
    $display("PASS: completion follows every full capture, including wrap and pause (width %0d, %0.1f ns)", COUNT_WIDTH, PERIOD_NS);
    $finish;
  end
  initial begin
    #(50*SAMPLES*PERIOD_NS);$fatal(1,"Test timed out");
  end
endmodule
