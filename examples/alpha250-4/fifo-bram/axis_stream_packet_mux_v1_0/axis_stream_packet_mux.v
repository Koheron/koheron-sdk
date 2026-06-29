// AXI4-Stream dual-input mux + fixed-length one-shot framer (AXI DMA friendly)
// Single AXI4-Lite CSR @0x00:
//   [0]     sel        (R/W) 0=s_axis_0, 1=s_axis_1
//   [1]     enable     (R/W) write 1 to send exactly one packet, auto-clears to 0 at end
//   [15:2]  length     (R/W) beats per packet (0 => 1)
// TLAST is asserted on the last beat of the packet (output only). Slave TLAST/TKEEP not used.
// Output TKEEP is all 1s (full-beat framing).

module axi_stream_packet_mux #(
    parameter integer DATA_WIDTH   = 32,
    parameter integer KEEP_WIDTH   = (DATA_WIDTH/8),
    parameter integer ADDR_WIDTH   = 4,   // >=4 (32-bit aligned)
    parameter integer LENGTH_LSB   = 2,   // length starts at bit 2
    parameter integer LENGTH_MSB   = 15
)(
    input  wire                      aclk,
    input  wire                      aresetn,

    // AXI4-Stream slave input 0 (no tkeep/tlast)
    input  wire [DATA_WIDTH-1:0]     s_axis_0_tdata,
    input  wire                      s_axis_0_tvalid,
    output wire                      s_axis_0_tready,

    // AXI4-Stream slave input 1 (no tkeep/tlast)
    input  wire [DATA_WIDTH-1:0]     s_axis_1_tdata,
    input  wire                      s_axis_1_tvalid,
    output wire                      s_axis_1_tready,

    // AXI4-Stream master output
    output wire [DATA_WIDTH-1:0]     m_axis_tdata,
    output wire [KEEP_WIDTH-1:0]     m_axis_tkeep,
    output wire                      m_axis_tvalid,
    input  wire                      m_axis_tready,
    output wire                      m_axis_tlast,

    // AXI4-Lite slave
    input  wire [ADDR_WIDTH-1:0]     s_axi_awaddr,
    input  wire                      s_axi_awvalid,
    output reg                       s_axi_awready,

    input  wire [31:0]               s_axi_wdata,
    input  wire [3:0]                s_axi_wstrb,
    input  wire                      s_axi_wvalid,
    output reg                       s_axi_wready,

    output reg  [1:0]                s_axi_bresp,
    output reg                       s_axi_bvalid,
    input  wire                      s_axi_bready,

    input  wire [ADDR_WIDTH-1:0]     s_axi_araddr,
    input  wire                      s_axi_arvalid,
    output reg                       s_axi_arready,

    output reg  [31:0]               s_axi_rdata,
    output reg  [1:0]                s_axi_rresp,
    output reg                       s_axi_rvalid,
    input  wire                      s_axi_rready
);

    // --------------- AXI-Lite ---------------
    localparam integer ADDR_LSB = 2; // 32-bit words
    localparam integer LENGTH_WIDTH = LENGTH_MSB - LENGTH_LSB + 1; // default 14

    wire wr_hs = s_axi_awvalid && s_axi_wvalid && s_axi_awready && s_axi_wready;
    wire rd_hs = s_axi_arvalid && s_axi_arready;

    reg                        cfg_sel;                 // bit 0
    reg                        cfg_enable;              // bit 1 (one-shot) - single owner block below
    reg  [LENGTH_WIDTH-1:0]    cfg_length;              // bits [15:2]

    // Address decode (only one 32-bit register at 0x00)
    wire addr_is_reg0 = (s_axi_awaddr[ADDR_WIDTH-1:ADDR_LSB] == { (ADDR_WIDTH-ADDR_LSB){1'b0} });
    wire ar_is_reg0   = (s_axi_araddr[ADDR_WIDTH-1:ADDR_LSB] == { (ADDR_WIDTH-ADDR_LSB){1'b0} });

    // Pulses from writes to bit[1] (enable)
    wire wr_set_enable = wr_hs && addr_is_reg0 && s_axi_wstrb[0] &&  s_axi_wdata[1];
    wire wr_clr_enable = wr_hs && addr_is_reg0 && s_axi_wstrb[0] && ~s_axi_wdata[1];

    // Write
    always @(posedge aclk) begin
        if (!aresetn) begin
            s_axi_awready <= 1'b0;
            s_axi_wready  <= 1'b0;
            s_axi_bvalid  <= 1'b0;
            s_axi_bresp   <= 2'b00;
            cfg_sel       <= 1'b0;
            cfg_length    <= {LENGTH_WIDTH{1'b0}};
        end else begin
            s_axi_awready <= (~s_axi_awready) && s_axi_awvalid && s_axi_wvalid;
            s_axi_wready  <= (~s_axi_wready)  && s_axi_awvalid && s_axi_wvalid;

            if (wr_hs && addr_is_reg0) begin
                // Byte lane 0: [7:0] -> sel(0), enable(1), length[5:0] -> [7:2]
                if (s_axi_wstrb[0]) begin
                    cfg_sel           <= s_axi_wdata[0];
                    // cfg_enable is handled by its own block via wr_set_enable/wr_clr_enable
                    cfg_length[5:0]   <= s_axi_wdata[7:2];
                end
                // Byte lane 1: [15:8] -> length[13:6]
                if (s_axi_wstrb[1]) begin
                    cfg_length[LENGTH_WIDTH-1:6] <= s_axi_wdata[15:8];
                end
                // Byte lanes 2..3: reserved
            end

            if (wr_hs && ~s_axi_bvalid) begin
                s_axi_bvalid <= 1'b1;
                s_axi_bresp  <= 2'b00; // OKAY
            end else if (s_axi_bvalid && s_axi_bready) begin
                s_axi_bvalid <= 1'b0;
            end
        end
    end

    // Read
    wire [31:0] reg0_read = {
        16'd0,                              // [31:16]
        cfg_length[LENGTH_WIDTH-1:6],       // [15:8] length[13:6]
        {cfg_length[5:0], cfg_enable, cfg_sel} // [7:0] length[5:0], enable, sel
    };

    always @(posedge aclk) begin
        if (!aresetn) begin
            s_axi_arready <= 1'b0;
            s_axi_rvalid  <= 1'b0;
            s_axi_rresp   <= 2'b00;
            s_axi_rdata   <= 32'd0;
        end else begin
            s_axi_arready <= (~s_axi_arready) && s_axi_arvalid;
            if (rd_hs) begin
                s_axi_rdata <= ar_is_reg0 ? reg0_read : 32'd0;
                s_axi_rvalid <= 1'b1;
                s_axi_rresp  <= 2'b00; // OKAY
            end else if (s_axi_rvalid && s_axi_rready) begin
                s_axi_rvalid <= 1'b0;
            end
        end
    end

    // --------------- AXI-Stream datapath ---------------
    // Effective length: 0 => 1 beat
    wire [LENGTH_WIDTH-1:0] pkt_len_eff =
        (cfg_length == {LENGTH_WIDTH{1'b0}}) ? {{(LENGTH_WIDTH-1){1'b0}},1'b1} : cfg_length;

    reg                         active;
    reg [LENGTH_WIDTH-1:0]      remaining;
    reg                         sel_latched;

    // Current selected input (latched per packet)
    wire cur_sel   = active ? sel_latched : cfg_sel;
    wire in_valid  = cur_sel ? s_axis_1_tvalid : s_axis_0_tvalid;
    wire want_data = cfg_enable && m_axis_tready;

    assign s_axis_0_tready = want_data && (cur_sel == 1'b0);
    assign s_axis_1_tready = want_data && (cur_sel == 1'b1);

    assign m_axis_tvalid = cfg_enable && in_valid;
    assign m_axis_tdata  = cur_sel ? s_axis_1_tdata : s_axis_0_tdata;

    // Full-beat framing -> tkeep is all ones
    assign m_axis_tkeep  = {KEEP_WIDTH{1'b1}};

    wire do_hs = m_axis_tvalid && m_axis_tready;

    // TLAST when remaining==1 or single-beat start
    wire single = (pkt_len_eff == {{(LENGTH_WIDTH-1){1'b0}},1'b1});
    wire tlast_comb = ( active && (remaining == {{(LENGTH_WIDTH-1){1'b0}},1'b1}) ) ||
                      ( !active && cfg_enable && in_valid && single );

    assign m_axis_tlast = tlast_comb;

    // Single-owner block for cfg_enable (avoids multi-driver)
    // Auto-clear at end-of-packet or on write-0; set on write-1.
    wire pkt_last_now = do_hs && ( (active && (remaining == {{(LENGTH_WIDTH-1){1'b0}},1'b1})) ||
                                   (!active && single) );

    always @(posedge aclk) begin
        if (!aresetn) begin
            cfg_enable <= 1'b0;
        end else begin
            cfg_enable <= (cfg_enable | wr_set_enable) & ~(wr_clr_enable | pkt_last_now);
        end
    end

    // Packet FSM
    always @(posedge aclk) begin
        if (!aresetn) begin
            active      <= 1'b0;
            remaining   <= {LENGTH_WIDTH{1'b0}};
            sel_latched <= 1'b0;
        end else begin
            if (!active) begin
                // Idle: start on first accepted beat
                if (do_hs) begin
                    sel_latched <= cfg_sel;
                    if (single) begin
                        active     <= 1'b0; // one-beat packet completes immediately
                        remaining  <= {LENGTH_WIDTH{1'b0}};
                    end else begin
                        active     <= 1'b1;
                        remaining  <= pkt_len_eff - {{(LENGTH_WIDTH-1){1'b0}},1'b1};
                    end
                end
            end else begin
                // In packet
                if (do_hs) begin
                    if (remaining == {{(LENGTH_WIDTH-1){1'b0}},1'b1}) begin
                        active     <= 1'b0;
                        remaining  <= {LENGTH_WIDTH{1'b0}};
                    end else begin
                        remaining  <= remaining - {{(LENGTH_WIDTH-1){1'b0}},1'b1};
                    end
                end
            end

            // If software clears enable mid-packet, abort on next cycle (cleanly)
            if (!cfg_enable) begin
                active    <= 1'b0;
                remaining <= {LENGTH_WIDTH{1'b0}};
            end
        end
    end

endmodule
