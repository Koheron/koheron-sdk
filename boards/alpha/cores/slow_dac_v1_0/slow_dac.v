`timescale 1 ns / 1 ps

// LDAC is pulsed low after the 8 DAC registers are written

module slow_dac #
(
  parameter CLK_DIV = 3
)
(
  input wire                      clk,
  input wire [16*4-1:0]           data,
  input wire                      valid,
  input wire [4-1:0]              cmd,

  output reg                      sync,
  output reg                      sclk,
  output reg [7-1:0]              cnt_sclk_out,
  output reg                      sdi,
  output reg                      ldac
);
  reg [16*8-1:0] data_reg;
  reg [4-1:0] cmd_reg;
  reg [CLK_DIV-1:0] cnt_clk  = 0; // Divide by 2**CLK_DIV the clock frequency
  reg [7-1: 0] cnt_sclk = 0;
  initial sync = 1'b1;

  always @(posedge clk) begin
    if (valid == 1'b1) begin
      cnt_clk <= cnt_clk + 1;

      if (cnt_clk == {{1'b0}, {(CLK_DIV-1){1'b1}}}) begin
        sclk <= 1'b1;
        if (cnt_sclk == {(7){1'b0}}) begin
          ldac <= 1'b1;
          cmd_reg <= cmd;
          data_reg <= data;
        end

        if (cnt_sclk[5-1:0] == {(5){1'b0}}) begin
          sync <= 1'b0;
        end

        cnt_sclk <= cnt_sclk + 1;

        if (sync == 1'b0 | cnt_sclk[5-1:0] == {(5){1'b0}}) begin
          if (cnt_sclk[5-1:2] == 3'b000) begin
            sdi <= cmd[3-cnt_sclk[2-1:0]]; // write command bits
          end else if (cnt_sclk[5-1:2] == 3'b001) begin
            sdi <= (cnt_sclk[7-1:5] == ~cnt_sclk[2-1:0]);
          end else if (cnt_sclk[5-1:0] == 5'b11000) begin
            sync <= 1'b1;
            if (cnt_sclk[7-1:5] == 3'b11) begin
              ldac <= 1'b0;
            end
          end else begin
            sdi <= data_reg[16*cnt_sclk[7-1:5] + 23 - cnt_sclk[5-1:0]];
          end
        end
        cnt_sclk_out <= cnt_sclk;
      end
      if (cnt_clk == {(CLK_DIV){1'b1}}) begin
        //cnt_clk <= 2'b00;
        sclk <= 1'b0;
      end
    end else begin // valid == 1'b0
      cnt_clk <= 0;
      cnt_sclk <= 0;
      sync <= 1;
      sdi <= 0;
      ldac <= 0;
      sclk <= 0;
    end
  end


endmodule
