module adc_overflow_counter (
    input wire clk,
    input wire resetn,
    input wire sample_valid,
    input wire sample_ready,
    output reg [31:0] lost_count = 0
);
    always @(posedge clk) begin
        if (!resetn)
            lost_count <= 0;
        else if (sample_valid && !sample_ready)
            lost_count <= lost_count + 1'b1;
    end
endmodule
