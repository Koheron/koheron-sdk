module axis_trig_gate #(
    parameter integer TDATA_WIDTH = 64
) (
    input  wire                         aclk,
    input  wire                         aresetn,

    input  wire                         trig,

    input  wire                         s_axis_tvalid,
    output wire                         s_axis_tready,
    input  wire [TDATA_WIDTH-1:0]       s_axis_tdata,
    input  wire [(TDATA_WIDTH+7)/8-1:0] s_axis_tkeep,
    input  wire                         s_axis_tlast,

    output wire                         m_axis_tvalid,
    input  wire                         m_axis_tready,
    output wire [TDATA_WIDTH-1:0]       m_axis_tdata,
    output wire [(TDATA_WIDTH+7)/8-1:0] m_axis_tkeep,
    output wire                         m_axis_tlast
);

    reg trig_d;
    wire trig_rise = trig & ~trig_d;

    reg gate_open;

    always @(posedge aclk) begin
        if (!aresetn) begin
            trig_d <= 1'b0;
            gate_open <= 1'b0;
        end else begin
            trig_d <= trig;
            if (trig_rise) begin
                gate_open <= 1'b1;
            end
        end
    end

    assign s_axis_tready = gate_open & m_axis_tready;
    assign m_axis_tvalid = gate_open & s_axis_tvalid;
    assign m_axis_tdata  = s_axis_tdata;
    assign m_axis_tkeep  = s_axis_tkeep;
    assign m_axis_tlast  = s_axis_tlast;

endmodule
