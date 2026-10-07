`timescale 1 ns / 1 ps

module phase_unwrapper #
(
  parameter integer DIN_WIDTH = 16,
  parameter integer DOUT_WIDTH = 32,
  parameter integer PIPELINED_OVERFLOW = 0,
  parameter integer FUSED_DIFFERENCE = 0,
  parameter integer CANONICAL_INPUT = 0
)
(
  input  wire clk,
  input  wire acc_on,
  input  wire rst,
  input  wire signed [DIN_WIDTH-1:0] phase_in,
  output wire signed [DIN_WIDTH+1-1:0] freq_out,
  output reg signed [DOUT_WIDTH-1:0] phase_out,
  output reg overflow
);

  // Value of Pi in scaled radians representation
  localparam PI = 2**(DIN_WIDTH-3);
  localparam TWOPI = 2**(DIN_WIDTH-2);

  reg signed [DIN_WIDTH-1:0] phase_in0;
  reg signed [DIN_WIDTH+1-1:0] diff;
  reg signed [DIN_WIDTH+1-1:0] unwrapped_diff;
  // The DPLL extractor registers its complete angle in the final DSP. Its
  // additional output clock is offset by fusing subtraction with unwrap,
  // keeping detector frequency and accumulated phase on their original cycles.
  // The default retains the existing three-clock pipeline for other designs.

  initial phase_out = 0;
  initial unwrapped_diff = 0;
  initial phase_in0 = 0;
  initial diff = 0;
  initial overflow = 0;

  wire signed [DOUT_WIDTH:0] next_phase =
      {phase_out[DOUT_WIDTH-1], phase_out} +
      {{(DOUT_WIDTH-DIN_WIDTH){unwrapped_diff[DIN_WIDTH]}}, unwrapped_diff};

  // Compute phase difference
  always @(posedge clk) begin
    phase_in0 <= phase_in;
    diff <= phase_in - phase_in0;
  end

  // Unwrap phase difference. TWOPI is a power of two, so wrapping only
  // changes the three upper bits. Fusing the low subtraction with a small
  // upper-bit lookup avoids a full-width compare/add chain at 250 MHz.
  generate if (FUSED_DIFFERENCE && CANONICAL_INPUT) begin : canonical_unwrap
    localparam LOW_WIDTH=DIN_WIDTH-2;
    // The custom extractor sign-extends its canonical [-pi,pi) angle.
    // Subtraction modulo 2*pi already wraps the low bits. Preserve the
    // original positive-pi difference tie rather than sign-extending it.
    wire [LOW_WIDTH-1:0] difference=phase_in[LOW_WIDTH-1:0]-phase_in0[LOW_WIDTH-1:0];
    wire positive_pi_tie=~phase_in[LOW_WIDTH-1] && phase_in0[LOW_WIDTH-1] &&
                        phase_in[LOW_WIDTH-2:0]==phase_in0[LOW_WIDTH-2:0];
    wire difference_sign=difference[LOW_WIDTH-1] && !positive_pi_tie;
    always @(posedge clk)
      unwrapped_diff <= {{3{difference_sign}}, difference};
`ifndef SYNTHESIS
    always @(posedge clk)
      if(phase_in[DIN_WIDTH-1:LOW_WIDTH] !== {2{phase_in[LOW_WIDTH-1]}})
        $fatal(1, "CANONICAL_INPUT requires a sign-extended [-pi,pi) angle");
`endif
  end else if (FUSED_DIFFERENCE) begin : fused_unwrap
    localparam LOW_WIDTH = DIN_WIDTH-2;
    wire [LOW_WIDTH:0] low_difference =
        {1'b0, phase_in[LOW_WIDTH-1:0]} -
        {1'b0, phase_in0[LOW_WIDTH-1:0]};
    wire lower_nonzero = phase_in[LOW_WIDTH-2:0] != phase_in0[LOW_WIDTH-2:0];
    wire [6:0] upper_address = {
        phase_in[DIN_WIDTH-1:LOW_WIDTH], phase_in0[DIN_WIDTH-1:LOW_WIDTH],
        low_difference[LOW_WIDTH], low_difference[LOW_WIDTH-1], lower_nonzero};
    function [127:0] make_upper_table(input integer plane);
      integer address, a, b, high;
      begin
        for (address=0; address<128; address=address+1) begin
          a=(address>>5)&3; if (a>=2) a=a-4;
          b=(address>>3)&3; if (b>=2) b=b-4;
          high=a-b-((address>>2)&1);
          if (high>0 || (high==0 && (address&2)!=0 && (address&1)!=0))
            high=high-1;
          else if (high < -1 || (high == -1 && (address&2)==0))
            high=high+1;
          make_upper_table[address]=high[plane];
        end
      end
    endfunction
    localparam [127:0] UPPER_0=make_upper_table(0);
    localparam [127:0] UPPER_1=make_upper_table(1);
    localparam [127:0] UPPER_2=make_upper_table(2);
    wire [2:0] upper_difference={UPPER_2[upper_address],
                               UPPER_1[upper_address], UPPER_0[upper_address]};
    always @(posedge clk)
      unwrapped_diff <= {upper_difference, low_difference[LOW_WIDTH-1:0]};
  end else begin : standard_unwrap
    always @(posedge clk) begin
      if (diff > PI) begin
        unwrapped_diff <= diff - TWOPI;
      end else if (diff < -PI) begin
        unwrapped_diff <= diff + TWOPI;
      end else begin
        unwrapped_diff <= diff;
      end
    end
  end endgenerate

  // Accumulate phase
  always @(posedge clk) begin
    if (rst) begin
      phase_out <= 0;
    end else begin
      if (acc_on) begin
        phase_out <= next_phase[DOUT_WIDTH-1:0];
      end else begin
        phase_out <= phase_out;
      end
    end
  end

  generate if (PIPELINED_OVERFLOW) begin : delayed_overflow
    // Check signs after the sum has been registered. Only the sticky error
    // flag is delayed; phase and frequency samples retain their latency.
    // PNA filtering delays the affected sample well beyond this one clock.
    reg previous_sign=0, difference_sign=0, accumulated=0;
    always @(posedge clk) begin
      if (rst) begin
        previous_sign <= 0;
        difference_sign <= 0;
        accumulated <= 0;
        overflow <= 0;
      end else begin
        previous_sign <= phase_out[DOUT_WIDTH-1];
        difference_sign <= unwrapped_diff[DIN_WIDTH];
        accumulated <= acc_on;
        overflow <= overflow | (accumulated && previous_sign==difference_sign &&
                               phase_out[DOUT_WIDTH-1]!=previous_sign);
      end
    end
  end else begin : immediate_overflow
    always @(posedge clk) begin
      if (rst) overflow <= 0;
      else if (acc_on) overflow <= overflow | (next_phase[DOUT_WIDTH] != next_phase[DOUT_WIDTH-1]);
    end
  end endgenerate

  assign freq_out = unwrapped_diff;

endmodule
