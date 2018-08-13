`timescale 1 ns / 1 ps

module unrandomizer #
(
  parameter integer DATA_WIDTH = 16
)
(
  input  wire [DATA_WIDTH-1: 0] din,
  output wire [DATA_WIDTH-1: 0] dout  
);

localparam integer HDW = DATA_WIDTH / 2; // Half data width

assign dout[0] = din[0];
assign dout[1] = din[HDW] ^ din[0];

genvar i;
generate
for (i = 1; i < HDW; i = i + 1) begin: XOR
  // XOR with din[0]
  assign dout[2*i] = din[i] ^ din[0];
  assign dout[2*i + 1] = din[HDW + i] ^ din[0];
end
endgenerate

endmodule

