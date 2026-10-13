`timescale 1 ns / 1 ps

// Simple counter for BRAM addressing
//
// A trigger queues one complete buffer, starting at the next address wrap.
// capture_complete toggles on the clock edge that writes the final sample.

module address_counter #
(
  parameter integer COUNT_WIDTH = 13
)
(
  input  wire clken, // Clock enable
  input  wire trig, // Trigger
  input  wire clk,
  output wire [31:0] address,
  output wire [3:0] wen, // Write enable
  output reg capture_complete
);

  localparam count_max = (1 << COUNT_WIDTH) - 1;
  reg trig_reg;
  reg trig_detected;

  reg wen_reg;

  reg [COUNT_WIDTH-1:0] count;

  initial count = 0;
  initial trig_reg = 0;
  initial trig_detected = 0;
  initial wen_reg = 0;
  initial capture_complete = 0;

  always @(posedge clk) begin
    trig_reg <= trig;
    // Rising edge detection
    if (trig & ~trig_reg) begin
        trig_detected <= 1;
    end else if (clken && count == count_max) begin
        trig_detected <= 0;
    end
  end

  // Register the carry one enabled cycle before the low byte wraps. This
  // keeps the BRAM address fanout off the long carry chain to the upper bits.
  generate if (COUNT_WIDTH > 8) begin : segmented_count
    reg carry_to_high;
    initial carry_to_high = 0;

    always @(posedge clk) begin
      if (clken) begin
        count[7:0] <= count[7:0] + 1'b1;
        count[COUNT_WIDTH-1:8] <= count[COUNT_WIDTH-1:8] + carry_to_high;
        carry_to_high <= (count[7:0] == 8'hfe);
      end
    end
  end else begin : simple_count
    always @(posedge clk) begin
      if (clken) begin
        count <= count + 1'b1;
      end
    end
  end endgenerate

  always @(posedge clk) begin
    if (clken && count == count_max) begin
      if (wen_reg) capture_complete <= ~capture_complete;
      wen_reg <= trig_detected;
    end
  end

  assign address = count << 2;
  assign wen = {4{wen_reg && clken}};

endmodule
