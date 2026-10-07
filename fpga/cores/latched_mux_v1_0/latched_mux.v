`timescale 1 ns / 1 ps

module latched_mux #
(
  parameter integer WIDTH = 32,
  parameter integer N_INPUTS = 3,
  parameter integer SEL_WIDTH = 2,
  parameter integer OUTPUT_STAGES = 1
)
(
  input  wire                          clk,
  input  wire                          clken,
  input  wire [(N_INPUTS * WIDTH)-1:0] din,
  input  wire [SEL_WIDTH-1 :0]         sel,
  output reg  [WIDTH-1:0]              dout
);

  reg [SEL_WIDTH-1 :0] sel_reg;
  initial sel_reg = {(SEL_WIDTH){1'b0}};

  always @(posedge clk) begin
    if (clken == 1'b1) begin
       sel_reg <= sel;
    end
  end

  wire [WIDTH-1:0] selected;
  generate if (OUTPUT_STAGES == 2) begin : output_pipeline
    reg [WIDTH-1:0] mux_word = 0;
    always @(posedge clk) mux_word <= din[sel_reg * WIDTH +: WIDTH];
    assign selected = mux_word;
  end else begin : direct_output
    assign selected = din[sel_reg * WIDTH +: WIDTH];
  end endgenerate

  initial if (OUTPUT_STAGES != 1 && OUTPUT_STAGES != 2)
    $error("Latched mux OUTPUT_STAGES must be 1 or 2");

  always @(posedge clk) begin
    dout <= selected;
  end

endmodule

