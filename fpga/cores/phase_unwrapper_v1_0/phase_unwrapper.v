`timescale 1 ns / 1 ps

module phase_unwrapper #
(
  parameter integer DIN_WIDTH = 16,
  parameter integer DOUT_WIDTH = 32
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

  // Unwrap phase difference
  always @(posedge clk) begin
    if (diff > PI) begin
      unwrapped_diff <= diff - TWOPI;
    end else if (diff < -PI) begin
      unwrapped_diff <= diff + TWOPI;
    end else begin
      unwrapped_diff <= diff;
    end
  end

  // Accumulate phase
  always @(posedge clk) begin
    if (rst) begin
      phase_out <= 0;
      overflow <= 0;
    end else begin
      if (acc_on) begin
        phase_out <= next_phase[DOUT_WIDTH-1:0];
        // Sticky until the packet's phase reset, even if phase wraps back.
        overflow <= overflow | (next_phase[DOUT_WIDTH] != next_phase[DOUT_WIDTH-1]);
      end else begin
        phase_out <= phase_out;
      end
    end
  end

  assign freq_out = unwrapped_diff;

endmodule
