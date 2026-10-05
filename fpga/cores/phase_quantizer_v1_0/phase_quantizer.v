`timescale 1 ns / 1 ps

// BASE_SHIFT accounts for extra CORDIC and filter fractional bits relative to
// the original phase scale. Latch precision, round to even, then saturate
// into a signed 32-bit sample. Metadata describes the last completed packet.
module phase_quantizer #(
    parameter integer PKT_LENGTH = 262144,
    parameter integer BASE_SHIFT = 8
) (
    input wire aclk,
    input wire aresetn,
    input wire [3:0] requested_bits,
    input wire upstream_overflow,
    input wire [39:0] s_axis_tdata,
    input wire s_axis_tvalid,
    output wire s_axis_tready,
    output reg [31:0] m_axis_tdata,
    output reg m_axis_tvalid,
    input wire m_axis_tready,
    output reg m_axis_tlast,
    output wire [4:0] sample_status,
    output reg [31:0] packet_status
);
    localparam COUNT_WIDTH = $clog2(PKT_LENGTH);
    reg [COUNT_WIDTH-1:0] input_count;
    reg [3:0] active_bits;
    wire [3:0] new_bits = requested_bits <= 8 ? requested_bits : 4'd0;
    wire [3:0] bits = input_count == 0 ? new_bits : active_bits;
    // Register packet selection before the shift/rounding logic. This keeps
    // the packet counter and live control bus out of the arithmetic path.
    reg [39:0] input_data;
    reg [3:0] input_bits;
    reg input_last, input_valid, input_overflow;
    wire [$clog2(BASE_SHIFT+1)-1:0] shift = BASE_SHIFT - input_bits;
    wire signed [39:0] quotient = $signed(input_data) >>> shift;
    wire [8:0] increments;
    genvar extra;
    generate for (extra = 0; extra <= 8; extra = extra + 1) begin : rounding
        localparam DROP = BASE_SHIFT - extra;
        if (DROP == 0) assign increments[extra] = 1'b0;
        else if (DROP == 1)
            assign increments[extra] = input_data[0] && input_data[1];
        else
            assign increments[extra] = input_data[DROP-1] &&
                ((|input_data[DROP-2:0]) || input_data[DROP]);
    end endgenerate

    // Three pipeline stages: packet selection, shift/round decision, saturation.
    reg signed [39:0] quotient_reg;
    reg increment_reg, last_reg, valid_reg, upstream_overflow_reg;
    reg [3:0] bits_reg, output_bits;
    reg output_overflow, packet_overflow;
    reg [23:0] packet_sequence;
    assign sample_status = {output_overflow, output_bits};
    wire advance = m_axis_tready || !m_axis_tvalid;
    assign s_axis_tready = advance;
    wire signed [40:0] rounded = {quotient_reg[39], quotient_reg} +
                                 $signed({40'd0, increment_reg});
    wire overflow = rounded[40:31] != {10{rounded[31]}};

    always @(posedge aclk) begin
        if (!aresetn) begin
            input_count <= 0;
            active_bits <= 0;
            input_valid <= 0;
            valid_reg <= 0;
            m_axis_tvalid <= 0;
            m_axis_tdata <= 0;
            m_axis_tlast <= 0;
            output_overflow <= 0;
            packet_overflow <= 0;
            packet_sequence <= 0;
            packet_status <= 0;
        end else begin
            if (m_axis_tvalid && m_axis_tready) begin
                if (m_axis_tlast) begin
                    packet_sequence <= packet_sequence + 1'b1;
                    packet_status <= {packet_sequence + 24'd1, 3'b000,
                                      packet_overflow | output_overflow, output_bits};
                    packet_overflow <= 0;
                end else if (output_overflow) begin
                    packet_overflow <= 1;
                end
            end
            if (advance) begin
                m_axis_tvalid <= valid_reg;
                m_axis_tlast <= last_reg;
                output_bits <= bits_reg;
                output_overflow <= overflow | upstream_overflow_reg;
                m_axis_tdata <= overflow ? (rounded[40] ? 32'h80000000 : 32'h7fffffff)
                                        : rounded[31:0];
                valid_reg <= input_valid;
                quotient_reg <= quotient;
                increment_reg <= increments[input_bits];
                last_reg <= input_last;
                bits_reg <= input_bits;
                upstream_overflow_reg <= input_overflow;
                input_valid <= s_axis_tvalid;
                if (s_axis_tvalid) begin
                    input_data <= s_axis_tdata;
                    input_last <= input_count == PKT_LENGTH - 1;
                    input_bits <= bits;
                    input_overflow <= upstream_overflow;
                    if (input_count == 0) active_bits <= new_bits;
                    input_count <= input_count == PKT_LENGTH - 1 ? 0 : input_count + 1'b1;
                end
            end
        end
    end
endmodule
