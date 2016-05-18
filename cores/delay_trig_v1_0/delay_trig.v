`timescale 1 ns / 1 ps

// Delay trigger until valid

module delay_trig #
(
)
(
  input wire clk,
  input wire trig_in,
  input wire valid,
  output reg trig_out,
  output reg ready
);

  reg ready_reg;

  initial begin
    ready <= 1;
    trig_out <= 0;
  end

  always @(posedge clk) begin
    ready <= ready_reg;
    if (ready) begin
      trig_out <= 0;
      if (trig_in) begin
        ready_reg <= 0;
      end
    end else begin
      if (valid) begin 
        trig_out <= 1;
        ready_reg <= 1;
      end
    end
  end

endmodule


