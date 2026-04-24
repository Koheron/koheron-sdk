module axis_packetizer #(
    parameter integer TDATA_WIDTH = 64,
    parameter integer PKT_LENGTH  = 32768
)(
    input                            aclk,
    input                            aresetn,

    input                            trig,
    input  [31:0]                    pkt_count,

    input                            s_axis_tvalid,
    output                           s_axis_tready,
    input  [TDATA_WIDTH-1:0]         s_axis_tdata,

    output                           m_axis_tvalid,
    input                            m_axis_tready,
    output                           m_axis_tlast,
    output [TDATA_WIDTH-1:0]         m_axis_tdata,
    output [(TDATA_WIDTH+7)/8-1:0]   m_axis_tkeep
);

    // rising edge detect
    reg trig_d;
    wire trig_rise = trig & ~trig_d;

    // latch pkt_count at trigger
    reg [31:0] pkt_count_m1;

    // state + counters
    reg packet_active;
    localparam integer BEAT_W = $clog2(PKT_LENGTH);
    reg [BEAT_W-1:0] beat_cnt;
    reg [31:0]       pkt_cnt;

    wire beat_xfer   = s_axis_tvalid & s_axis_tready;
    wire last_beat   = (beat_cnt == PKT_LENGTH-1);
    wire last_packet = (pkt_cnt  == pkt_count_m1);

    always @(posedge aclk) begin
        if (!aresetn) begin
            trig_d         <= 1'b0;
            pkt_count_m1   <= 32'd0;
            packet_active  <= 1'b0;
            beat_cnt       <= {BEAT_W{1'b0}};
            pkt_cnt        <= 32'd0;
        end else begin
            trig_d <= trig;

            if (trig_rise) begin
                pkt_count_m1  <= pkt_count - 32'd1;
                packet_active <= 1'b1;
                beat_cnt      <= {BEAT_W{1'b0}};
                pkt_cnt       <= 32'd0;
            end else if (packet_active && beat_xfer) begin
                if (last_beat) begin
                    beat_cnt <= {BEAT_W{1'b0}};
                    if (last_packet)
                        packet_active <= 1'b0;
                    else
                        pkt_cnt <= pkt_cnt + 32'd1;
                end else begin
                    beat_cnt <= beat_cnt + 1'b1;
                end
            end
        end
    end

    // AXIS passthrough
    assign s_axis_tready = packet_active & m_axis_tready;
    assign m_axis_tvalid = packet_active & s_axis_tvalid;
    assign m_axis_tdata  = s_axis_tdata;
    assign m_axis_tlast  = packet_active & last_beat;

    localparam integer TKEEP_WIDTH = (TDATA_WIDTH+7)/8;
    assign m_axis_tkeep = {TKEEP_WIDTH{1'b1}};

endmodule
