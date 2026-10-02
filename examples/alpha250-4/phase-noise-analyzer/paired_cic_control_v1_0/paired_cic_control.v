`timescale 1ns/1ps
// The phase differences are live ADC-clock signals, not buffered AXI sources.
// Admit a sample only when BOTH decimators can consume that ADC-clock instant.
// Otherwise independently stalled CICs silently decimate different time axes.
module paired_cic_control (
    input wire aclk,
    input wire aresetn,
    input wire [15:0] requested_rate,
    input wire data_ready_x,
    input wire data_ready_y,
    output wire data_valid,
    input wire config_ready_x,
    input wire config_ready_y,
    output wire config_valid,
    output wire [15:0] config_rate,
    output wire filter_resetn
);
    localparam WAIT_RATE=0, RESET=1, CONFIGURE=2, PRIME=3, STREAM=4;
    reg [2:0] state=WAIT_RATE;
    reg [5:0] reset_count=0;
    reg [15:0] rate=0;
    wire valid_rate = requested_rate >= 4 && requested_rate <= 8192;
    // Reset both CIC/FIR histories and FIFO queues, not only the rate registers.
    // 32 ADC clocks also covers the async FIFO's slower read-clock reset width.
    assign filter_resetn = aresetn && (state==CONFIGURE || state==PRIME || state==STREAM);
    assign data_valid = state==STREAM && requested_rate==rate && data_ready_x && data_ready_y;
    assign config_valid = state==CONFIGURE && config_ready_x && config_ready_y;
    assign config_rate = rate;
    always @(posedge aclk) begin
        if(!aresetn) begin
            state<=WAIT_RATE;
            reset_count<=0;
            rate<=0;
        end else if(valid_rate && requested_rate!=rate) begin
            rate<=requested_rate;
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
                PRIME: state<=STREAM;
                STREAM: begin end
                default: state<=WAIT_RATE;
            endcase
        end
    end
endmodule
