`timescale 1 ns / 1 ps

module edge_detector #
(
  parameter integer PULSE_WIDTH = 1
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

  generate
    if (PULSE_WIDTH == 1)
    begin : ONE
      assign dout = !din_next && din;
    end
    if (PULSE_WIDTH > 1)
    begin : MORE_THAN_ONE

      function integer clogb2 (input integer value);
        for(clogb2 = 0; value > 0; clogb2 = clogb2 + 1) value = value >> 1;
      endfunction
 
      localparam integer CNT_WIDTH = clogb2(PULSE_WIDTH);    
 
      reg [CNT_WIDTH-1:0] cnt;
      reg counting;
      always @(posedge clk) begin
        if (!din_next && din) begin
          cnt <= cnt + 1;
          counting <= 1;
        end else begin
          if (counting && cnt < PULSE_WIDTH) begin
            cnt <= cnt + 1;
          end else begin
            counting <= 0;
            cnt <= 0;
          end   
        end
      end
      assign dout = (counting || (!din_next && din));
    end
  endgenerate

endmodule

