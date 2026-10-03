`timescale 1 ns / 1 ps

// Forty-bit filter output has eight more fractional bits than the original
// phase format. Latch precision at packet start, round to even, then saturate
// into a signed 32-bit sample. Metadata describes the last completed packet.
module phase_quantizer #(
    parameter integer PKT_LENGTH = 262144
) (
    input wire aclk,
    input wire aresetn,
    input wire [3:0] requested_bits,
    input wire [39:0] s_axis_tdata,
    input wire s_axis_tvalid,
    output wire s_axis_tready,
    output reg [31:0] m_axis_tdata,
    output reg m_axis_tvalid,
    input wire m_axis_tready,
    output reg m_axis_tlast,
    output reg [31:0] packet_status
);
    localparam COUNT_WIDTH = $clog2(PKT_LENGTH);
    reg [COUNT_WIDTH-1:0] input_count;
    reg [3:0] active_bits;
    wire [3:0] new_bits = requested_bits <= 8 ? requested_bits : 4'd0;
    wire [3:0] bits = input_count == 0 ? new_bits : active_bits;
    wire [3:0] shift = 4'd8 - bits;
    wire signed [39:0] quotient = $signed(s_axis_tdata) >>> shift;
    wire [8:0] mask = (9'd1 << shift) - 9'd1;
    wire [8:0] remainder = {1'b0, s_axis_tdata[7:0]} & mask;
    wire [8:0] half = (9'd1 << shift) >> 1;
    wire increment = shift != 0 &&
        (remainder > half || (remainder == half && quotient[0]));

    // Two elastic pipeline stages separate variable shifting from rounding.
    reg signed [39:0] quotient_reg;
    reg increment_reg, last_reg, valid_reg;
    reg [3:0] bits_reg, output_bits;
    reg output_overflow, packet_overflow;
    reg [23:0] packet_sequence;
    wire advance = m_axis_tready || !m_axis_tvalid;
    assign s_axis_tready = advance;
    wire signed [40:0] rounded = {quotient_reg[39], quotient_reg} +
                                 $signed({40'd0, increment_reg});
    wire overflow = rounded[40:31] != {10{rounded[31]}};

    always @(posedge aclk) begin
        if (!aresetn) begin
            input_count <= 0;
            active_bits <= 0;
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
                output_overflow <= overflow;
                m_axis_tdata <= overflow ? (rounded[40] ? 32'h80000000 : 32'h7fffffff)
                                        : rounded[31:0];
                valid_reg <= s_axis_tvalid;
                if (s_axis_tvalid) begin
                    quotient_reg <= quotient;
                    increment_reg <= increment;
                    last_reg <= input_count == PKT_LENGTH - 1;
                    bits_reg <= bits;
                    if (input_count == 0) active_bits <= new_bits;
                    input_count <= input_count == PKT_LENGTH - 1 ? 0 : input_count + 1'b1;
                end
            end
        end
    end
endmodule
