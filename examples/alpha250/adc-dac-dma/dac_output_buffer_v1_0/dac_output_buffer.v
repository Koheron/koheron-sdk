`timescale 1ns / 1ps

// The ADC and DAC clocks have the same frequency but a programmable phase
// difference. Buffer their handoff rather than using a short inter-clock path.
module dac_output_buffer #(
    parameter integer WIDTH = 16
)(
    input wire adc_clk,
    input wire dac_clk,
    input wire reset,
    input wire [WIDTH-1:0] din,
    (* DONT_TOUCH = "true" *) output reg [WIDTH-1:0] dout0 = {WIDTH{1'b0}},
    (* DONT_TOUCH = "true" *) output reg [WIDTH-1:0] dout1 = {WIDTH{1'b0}}
);
    wire [WIDTH-1:0] fifo_word;
    wire fifo_full, fifo_empty, wr_busy, rd_busy;
    xpm_fifo_async #(
        .FIFO_MEMORY_TYPE("distributed"), .ECC_MODE("no_ecc"),
        .RELATED_CLOCKS(0), .SIM_ASSERT_CHK(1),
        .FIFO_WRITE_DEPTH(16), .WRITE_DATA_WIDTH(WIDTH), .READ_DATA_WIDTH(WIDTH),
        .WR_DATA_COUNT_WIDTH(5), .RD_DATA_COUNT_WIDTH(5),
        .FULL_RESET_VALUE(0), .USE_ADV_FEATURES("0101"),
        .READ_MODE("fwft"), .FIFO_READ_LATENCY(0), .DOUT_RESET_VALUE("0"),
        .CDC_SYNC_STAGES(2), .WAKEUP_TIME(0)
    ) fifo (
        .rst(reset), .sleep(1'b0),
        .wr_clk(adc_clk), .din(din), .wr_en(!reset && !wr_busy && !fifo_full),
        .full(fifo_full), .wr_rst_busy(wr_busy),
        .rd_clk(dac_clk), .rd_en(!rd_busy && !fifo_empty),
        .dout(fifo_word), .empty(fifo_empty), .rd_rst_busy(rd_busy),
        .injectsbiterr(1'b0), .injectdbiterr(1'b0),
        .prog_full(), .wr_data_count(), .overflow(), .almost_full(), .wr_ack(),
        .prog_empty(), .rd_data_count(), .underflow(), .almost_empty(),
        .data_valid(), .sbiterr(), .dbiterr()
    );
    always @(posedge dac_clk) begin
        dout0 <= (rd_busy || fifo_empty) ? {WIDTH{1'b0}} : fifo_word;
        dout1 <= (rd_busy || fifo_empty) ? {WIDTH{1'b0}} : fifo_word;
    end
endmodule
