`timescale 1 ns / 1 ps

module axi_sts_register #
(
  parameter integer STS_DATA_WIDTH = 1024,
  parameter integer AXI_DATA_WIDTH = 32,
  parameter integer AXI_ADDR_WIDTH = 16
)
(
  input  wire                      aclk,
  input  wire                      aresetn,
  input  wire [STS_DATA_WIDTH-1:0] sts_data,
  input  wire [AXI_ADDR_WIDTH-1:0] s_axi_awaddr,
  input  wire                      s_axi_awvalid,
  output wire                      s_axi_awready,
  input  wire [AXI_DATA_WIDTH-1:0] s_axi_wdata,
  input  wire                      s_axi_wvalid,
  output wire                      s_axi_wready,
  output wire [1:0]                s_axi_bresp,
  output wire                      s_axi_bvalid,
  input  wire                      s_axi_bready,
  input  wire [AXI_ADDR_WIDTH-1:0] s_axi_araddr,
  input  wire                      s_axi_arvalid,
  output wire                      s_axi_arready,
  output wire [AXI_DATA_WIDTH-1:0] s_axi_rdata,
  output wire [1:0]                s_axi_rresp,
  output wire                      s_axi_rvalid,
  input  wire                      s_axi_rready
);

  function integer clogb2 (input integer v);
    begin
      v = v - 1; for (clogb2 = 0; v > 0; clogb2 = clogb2 + 1) v = v >> 1;
    end
  endfunction

  localparam integer ADDR_LSB = clogb2(AXI_DATA_WIDTH/8);
  localparam integer STS_SIZE = STS_DATA_WIDTH/AXI_DATA_WIDTH;

  reg aw_hold, w_hold, bvalid;
  reg [AXI_ADDR_WIDTH-1:0] awaddr_q;

  assign s_axi_awready = ~aw_hold;
  assign s_axi_wready  = ~w_hold;

  wire aw_fire = s_axi_awvalid & s_axi_awready;
  wire w_fire  = s_axi_wvalid  & s_axi_wready;

  always @(posedge aclk) begin
    if (~aresetn) begin
      aw_hold  <= 1'b0;
      w_hold   <= 1'b0;
      bvalid   <= 1'b0;
      awaddr_q <= {AXI_ADDR_WIDTH{1'b0}};
    end else begin
      if (aw_fire) begin aw_hold <= 1'b1; awaddr_q <= s_axi_awaddr; end
      if (w_fire ) begin w_hold  <= 1'b1; end
      if (aw_hold & w_hold & ~bvalid) begin
        bvalid  <= 1'b1;
        aw_hold <= 1'b0;
        w_hold  <= 1'b0;
      end
      if (bvalid & s_axi_bready) bvalid <= 1'b0;
    end
  end

  assign s_axi_bvalid = bvalid;
  assign s_axi_bresp  = 2'b00;

  reg ar_hold, rvalid;
  reg [AXI_ADDR_WIDTH-1:0] araddr_q;
  reg [AXI_DATA_WIDTH-1:0] rdata_q;

  assign s_axi_arready = ~ar_hold & ~rvalid;
  wire ar_fire = s_axi_arvalid & s_axi_arready;

  integer ridx;

  always @(posedge aclk) begin
    if (~aresetn) begin
      ar_hold  <= 1'b0;
      rvalid   <= 1'b0;
      araddr_q <= {AXI_ADDR_WIDTH{1'b0}};
      rdata_q  <= {AXI_DATA_WIDTH{1'b0}};
    end else begin
      if (ar_fire) begin
        ar_hold  <= 1'b1;
        araddr_q <= s_axi_araddr;
      end
      if (ar_hold & ~rvalid) begin
        ridx = araddr_q >> ADDR_LSB;
        if (ridx < STS_SIZE)
          rdata_q <= sts_data[ridx*AXI_DATA_WIDTH +: AXI_DATA_WIDTH];
        else
          rdata_q <= {AXI_DATA_WIDTH{1'b0}};
        rvalid  <= 1'b1;
        ar_hold <= 1'b0;
      end else if (rvalid & s_axi_rready) begin
        rvalid <= 1'b0;
      end
    end
  end

  assign s_axi_rvalid = rvalid;
  assign s_axi_rdata  = rdata_q;
  assign s_axi_rresp  = 2'b00;

endmodule
