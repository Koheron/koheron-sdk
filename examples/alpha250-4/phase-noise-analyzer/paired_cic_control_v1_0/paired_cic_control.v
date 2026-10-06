`timescale 1ns/1ps
// The phase differences are live ADC-clock signals, not buffered AXI sources.
// Admit a sample only when BOTH decimators can consume that ADC-clock instant.
// Otherwise independently stalled CICs silently decimate different time axes.
module paired_cic_control (
    input wire aclk,
    input wire aresetn,
    input wire [15:0] requested_rate,
    input wire [3:0] requested_bits,
    input wire requested_epoch,
    input wire requested_run,
    output wire [3:0] active_bits,
    input wire data_ready_x,
    input wire data_ready_y,
    output wire data_valid,
    input wire config_ready_x,
    input wire config_ready_y,
    output wire config_valid,
    output wire [15:0] config_rate,
    input wire upstream_overflow_x,
    input wire upstream_overflow_y,
    output reg overflow_x = 0,
    output reg overflow_y = 0,
    output wire filter_resetn,
    output reg sample_gap = 0
);
    localparam WAIT_RATE=0, RESET=1, CONFIGURE=2, PRIME=3, STREAM=4;
    reg [2:0] state=WAIT_RATE;
    reg [5:0] reset_count=0;
    reg [15:0] rate=0;
    reg [3:0] precision=0;
    reg epoch=0;
    assign active_bits = precision;
    wire valid_rate = requested_rate >= 4 && requested_rate <= 8192 && !requested_rate[0];
    // Reset both CIC/FIR histories and FIFO queues, not only the rate registers.
    // 32 ADC clocks also covers the async FIFO's slower read-clock reset width.
    assign filter_resetn = aresetn && requested_run && (state==CONFIGURE || state==PRIME || state==STREAM);
    // Changed settings reset both histories at the next edge. Keeping that
    // comparison in the state machine avoids a high-fanout filter-enable path.
    assign data_valid = state==STREAM && data_ready_x && data_ready_y;
    assign config_valid = state==CONFIGURE && config_ready_x && config_ready_y;
    assign config_rate = rate;
    always @(posedge aclk) begin
        if (!filter_resetn) sample_gap <= 0;
        else if (state==STREAM && (!data_ready_x || !data_ready_y)) sample_gap <= 1;
        if (!filter_resetn) begin
            overflow_x <= 0;
            overflow_y <= 0;
        end else begin
            overflow_x <= overflow_x | upstream_overflow_x;
            overflow_y <= overflow_y | upstream_overflow_y;
        end
    end
    always @(posedge aclk) begin
        if(!aresetn) begin
            state<=WAIT_RATE;
            reset_count<=0;
            rate<=0;
            precision<=0;
            epoch<=0;
        end else if(!requested_run) begin
            state<=RESET;
            reset_count<=0;
            rate<=requested_rate;
            precision<=requested_bits;
            epoch<=requested_epoch;
        end else if(valid_rate && requested_bits<=8 && (requested_rate!=rate || requested_bits!=precision || requested_epoch!=epoch)) begin
            rate<=requested_rate;
            precision<=requested_bits;
            epoch<=requested_epoch;
            reset_count<=0;
            state<=RESET;
        end else begin
            case(state)
                WAIT_RATE: begin end
                RESET: begin
                    if(reset_count==31) state<=CONFIGURE;
                    else reset_count<=reset_count+1'b1;
                end
                CONFIGURE: if(config_valid) state<=PRIME;
                PRIME: if(data_ready_x && data_ready_y) state<=STREAM;
                STREAM: begin end
                default: state<=WAIT_RATE;
            endcase
        end
    end
endmodule
