`timescale 1ns/1ps
// Metadata is stable throughout an acquisition epoch. Sticky error flags cross
// before the affected data traverses the CIC, converter and FIR pipelines.
module phase_stream_cdc #(
    parameter integer METADATA_WIDTH = 6
) (
    input wire clk,
    input wire status_clk,
    input wire resetn_in,
    input wire [METADATA_WIDTH-1:0] metadata_in,
    input wire [31:0] packet_in,
    input wire [15:0] rate_in,
    output wire resetn,
    output wire [METADATA_WIDTH-1:0] metadata,
    output wire [15:0] rate,
    output wire [31:0] packet_status
);
    xpm_cdc_async_rst #(
        .DEST_SYNC_FF(3), .RST_ACTIVE_HIGH(0)
    ) reset_sync (
        .src_arst(resetn_in), .dest_clk(clk), .dest_arst(resetn)
    );

    // The stream controller latches this word before its 32-clock reset
    // interval. It is stable well before the synchronized reset releases;
    // the slow CIC samples it only once at the start of each epoch.
    xpm_cdc_array_single #(
        .WIDTH(16), .DEST_SYNC_FF(3), .SRC_INPUT_REG(0)
    ) rate_sync (
        .src_clk(1'b0), .src_in(rate_in), .dest_clk(clk), .dest_out(rate)
    );
    xpm_cdc_array_single #(
        .WIDTH(METADATA_WIDTH), .DEST_SYNC_FF(3), .SRC_INPUT_REG(0)
    ) metadata_sync (
        .src_clk(1'b0), .src_in(metadata_in),
        .dest_clk(clk), .dest_out(metadata)
    );

    // The status-register bank runs on the ADC clock. Transfer its diagnostic
    // packet counter as one coherent word, not independently synchronized bits.
    reg [31:0] packet_held = 0;
    reg [1:0] status_state = 0;
    wire received;
    wire send_status = status_state == 1;
    always @(posedge clk) begin
        case (status_state)
            0: begin packet_held <= packet_in; status_state <= 1; end
            1: if (received) status_state <= 2;
            2: if (!received) status_state <= 0;
            default: status_state <= 0;
        endcase
    end
    xpm_cdc_handshake #(
        .WIDTH(32), .DEST_EXT_HSK(0), .DEST_SYNC_FF(3), .SRC_SYNC_FF(3)
    ) packet_sync (
        .src_clk(clk), .src_in(packet_held), .src_send(send_status), .src_rcv(received),
        .dest_clk(status_clk), .dest_out(packet_status), .dest_req(), .dest_ack(1'b0)
    );
endmodule
