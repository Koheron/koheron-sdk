`timescale 1 ns / 1 ps

module ad7124 #()
(
  input wire                  clk,
  input wire                  resetn,
  input wire                  sdo,
  output reg                  sclk,
  output reg                  cs,
  output reg                  sdi,
  output reg [24-1:0]         dout,
  output reg                  valid
);

reg [14-1:0] cnt;
reg [24-1:0] dout_reg;

initial cnt = 0;
initial cs = 1;
initial sclk = 1;
initial dout = 0;
initial dout_reg = 0;

localparam [8-1:0] READ_CODE = 8'b01000010;

always @(posedge clk) begin
    if (~resetn) begin
        cnt <= 0;
    end else begin
        cnt <= cnt + 1;
    end
end

always @(posedge clk) begin
    if (~resetn) begin
        cnt <= 0;
    end else begin
        if (cnt == 0) begin
            cs <= 0;
            sclk <= 0;
            valid <= 0;
        end else if (cnt == 512) begin
            cs <= 1;
        end
    end
end

always @(posedge clk) begin
    if (~resetn) begin
        cnt <= 0;
    end else begin
        if ((cnt[3:0] == 4'b0000) && (cs == 0)) begin
            sclk <= 0;
            if (cnt[14-1:4] < 8) begin
                sdi <= READ_CODE[7 - cnt[14-1:4]];
            end else if (cnt[14-1:4] < 32) begin
                sdi <= 0;
                dout_reg[32 - cnt[14-1:4]] <= sdo;
            end else if (cnt[14-1:4] < 32) begin
                dout <= dout_reg;
                valid <= 1;
            end
        end else if (cnt[3:0] == 4'b1000) begin
            sclk <= 1;
        end
    end
end


  
endmodule
