`timescale 1 ns / 1 ps

function integer clogb2 (input integer value);
  for(clogb2 = 0; value > 0; clogb2 = clogb2 + 1) value = value >> 1;
endfunction

module simple_interconnect #
(
  parameter integer WIDTH = 32,
  parameter integer N_INPUTS = 3,
  parameter integer N_OUTPUTS = 2
)
(
  input  wire [(N_INPUTS * WIDTH)-1:0]              in,
  input  wire [($clog2(N_INPUTS) * N_OUTPUTS)-1 :0] sel,
  output wire [(N_OUTPUTS * WIDTH)-1:0]             out
);

  genvar i;
  generate
    for (i = 0; i < N_OUTPUTS; i = i+1)     
    begin : gen_out
      assign out[(i+1)*WIDTH-1 : i*WIDTH] = 0;
    end  
  endgenerate

endmodule


