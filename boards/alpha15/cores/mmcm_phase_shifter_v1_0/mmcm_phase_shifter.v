`timescale 1 ns / 1 ps

module mmcm_phase_shifter #
()
(
  input wire clk,
  input wire [31:0] ctl,
  output reg psen,
  output wire psincdec,
  output wire done
);

wire en = ctl[2];
wire start;

reg [11:0] n_pulses;
reg [15:0] cnt;
reg en_next;

initial cnt = 0;

assign psincdec = ctl[3];
assign done = (cnt == 0);

always @(posedge clk) begin
  en_next <= en;
end

assign start = !en_next && en;

always @(posedge clk) begin
  if ((start == 1) && (cnt == 0)) begin
    cnt <= {ctl[15:4],4'b0000};
  end else if (cnt > 0) begin
    cnt <= cnt - 1;
  end else begin
    cnt <= 0;
  end
end

always @(posedge clk) begin
  if (cnt[3:0] == 4'b1111) begin
    psen <= 1;
  end else begin
    psen <= 0;
  end
end

endmodule