`timescale 1 ns / 1 ps

module dsp_counter #
(
  parameter integer PERIOD = 256,
  parameter integer PERIOD_WIDTH = 8,
  parameter integer N_CYCLES = 2048,
  parameter integer N_CYCLES_WIDTH = 11
)
(
  input wire clk,
  input wire s_axis_tvalid,
  input wire [32-1:0] s_axis_tdata,
  output wire m_axis_tvalid,
  output wire [32-1:0] m_axis_tdata,
  output reg [PERIOD_WIDTH+1:0] addr,
  output reg [N_CYCLES_WIDTH-1:0] cycle_index,
  output reg first_cycle,
  output reg last_cycle
);

reg [PERIOD_WIDTH-1:0] cnt;
reg [N_CYCLES_WIDTH-1:0] cycle_index_reg;

initial cnt = 0;
initial cycle_index_reg = 0;

always @(posedge clk) begin
  addr <= {{cnt}, {2'b0}};
  cycle_index <= cycle_index_reg;
  first_cycle <= ((cnt < PERIOD) && (cycle_index_reg == 0));
  last_cycle <= ((cnt < PERIOD) && (cycle_index_reg == N_CYCLES - 1));
end

always @(posedge clk) begin
  if (s_axis_tvalid) begin
    if (cnt < PERIOD - 1) begin
      cnt <= cnt + 1;
    end else begin
      cnt <= 0;
      if (cycle_index < N_CYCLES - 1) begin
        cycle_index_reg <= cycle_index_reg + 1;
      end else begin
        cycle_index_reg <= 0;
      end
    end
  end
end

assign m_axis_tvalid = s_axis_tvalid;
assign m_axis_tdata = s_axis_tdata;

endmodule
