`timescale 1 ns / 1 ps

module spi_cfg #
(
  parameter CLK_DIV = 3,
  parameter N_SLAVES = 4
)
(
  input wire                      aclk,
  input wire [32-1:0]             s_axis_tdata,
  input wire                      s_axis_tvalid,
  input wire [8-1:0]              cmd,

  output reg [N_SLAVES-1:0]       cs,
  output reg                      sclk,
  output reg                      sdi,
  output reg                      s_axis_tready
);
  reg [32-1:0] data_reg;
  reg [8-1:0] cmd_reg;
  reg [CLK_DIV-1:0] cnt_clk  = 0; // Divide by 2**CLK_DIV the clock frequency
  reg [6-1: 0] cnt_sclk;
  reg sclk_reg;
  reg end_sequence;
  initial s_axis_tready = 1'b1;
  initial cs = {(N_SLAVES){1'b1}};
  initial end_sequence = 1'b0;

  always @(posedge aclk) begin
    cnt_clk <= cnt_clk + 1;

    if (s_axis_tready == 1'b0) begin
      if (cnt_clk == {(CLK_DIV){1'b1}}) begin 
        // spi clock posedge
        cnt_sclk <= cnt_sclk + 1;
        sclk_reg <= 1'b1;
        sclk <= sclk_reg;
        if (end_sequence == 0) begin               
          if (cnt_sclk < 8) begin
            sdi <= data_reg[31 - cnt_sclk];
            if (cnt_sclk == 7 && cmd_reg[3:2] == 2'b00) begin // Stop at 1 byte written
              end_sequence <= 1'b1;
            end
          end else if (cnt_sclk >=  8 && cnt_sclk < 16) begin
            sdi <= data_reg[31 - cnt_sclk];
            if (cnt_sclk == 15 && cmd_reg[3:2] == 2'b01) begin // Stop at 2 bytes written
              end_sequence <= 1'b1;
            end
          end else if (cnt_sclk >=  16 && cnt_sclk < 24) begin
            sdi <= data_reg[31 - cnt_sclk];
            if (cnt_sclk == 23 && cmd_reg[3:2] == 2'b10) begin // Stop at 3 bytes written
              end_sequence <= 1'b1;
            end
          end else if (cnt_sclk >=  24 && cnt_sclk < 32) begin
            sdi <= data_reg[31 - cnt_sclk];
            if (cnt_sclk == 31) begin // Stop at 4 bytes written
              end_sequence <= 1'b1;
            end
          end
        end else begin // end_sequence == 1
          sdi <= 0;
          sclk_reg <= 1'b0;
          cs <= {(N_SLAVES){1'b1}};
          s_axis_tready <= 1'b1;
          end_sequence <= 1'b0;
        end

      end else if (cnt_clk == {{1'b0}, {(CLK_DIV-1){1'b1}}}) begin
        // spi clock negedge
        sclk_reg <= 1'b0;
        sclk <= sclk_reg;
      end
    end else begin
      // (cs == 1'b1)
      if (s_axis_tvalid && s_axis_tready) begin
        cs[cmd[1:0]] <= 0;
        s_axis_tready <= 0;
        cmd_reg <= cmd;
        data_reg <= s_axis_tdata;
        cnt_clk <= 0;
        cnt_sclk <= 0;
        sclk_reg <= 0;
        sclk <= 0;
      end
    end
  end

endmodule
