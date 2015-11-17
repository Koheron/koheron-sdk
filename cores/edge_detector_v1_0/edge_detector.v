`timescale 1 ns / 1 ps

module edge_detector #
(
)
(
  input  wire din,
  input  wire clk,
  output wire dout   
);
  reg din_next;
  always @(posedge clk) begin
    din_next <= din;
  end
  assign dout = !din_next && din; 

endmodule

