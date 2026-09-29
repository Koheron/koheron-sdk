`timescale 1ns/1ps
module integration_tb;
  reg passed = 0;
  reg clk = 0;
  always #2 clk = ~clk;
  reg valid = 0;
  reg [31:0] data = 0;
  wire [31:0] sum, sum_addr;
  wire [3:0] wen;
  system_wrapper dut(.clk(clk), .data(data), .valid(valid), .sum(sum), .sum_addr(sum_addr), .wen(wen));
  integer sent=0, received=0, bin, batch;
  shortreal value, expected;
  reg [31:0] expected_bits;
  always @(posedge clk) begin
    if (wen === 4'hf && received < 64*3) begin
      bin=received%64;
      batch=received/64;
      // Each input frame has a distinct offset, so mixing frames is detectable.
      expected=3.0*(bin+1) + 100.0*(9*batch+6);
      expected_bits=$shortrealtobits(expected);
      if (sum_addr !== 4*bin || sum !== expected_bits)
        $fatal(1,"sum mismatch index=%0d addr=%0d value=%h expected=%h",received,sum_addr,sum,expected_bits);
      received=received+1;
    end
  end
  initial begin
    repeat (20) @(negedge clk);
    // Keep streaming beyond the checked averages so the adder stays enabled.
    for(sent=0;sent<64*3*3+32;sent=sent+1) begin
      valid=1;
      value=(sent%64)+1+100*((sent/64)+1);
      data=$shortrealtobits(value);
      @(negedge clk);
    end
    valid=0;
    repeat (40) @(negedge clk);
    if(received!=64*3) $fatal(1,"missing outputs: %0d", received);
    passed = 1;
    $display("PASS: PSD counter + vendor BRAM accumulator, 3 averages / 192 bins");
    $finish;
  end
  initial begin #100000; $fatal(1,"timeout"); end
endmodule
