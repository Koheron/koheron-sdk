module adc_sample_tag (
    input wire clk,
    input wire resetn,
    input wire [17:0] sample,
    input wire sample_valid,
    output wire [31:0] tagged_sample
);
    reg [13:0] sequence = 0;

    // Count ADC conversions, regardless of whether the DMA can accept them.
    // A dropped conversion therefore leaves a gap in the saved sequence.
    always @(posedge clk) begin
        if (!resetn)
            sequence <= 0;
        else if (sample_valid)
            sequence <= sequence + 1'b1;
    end

    assign tagged_sample = {sequence, sample};
endmodule
