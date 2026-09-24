module dac_test_tone (
    input wire clk,
    input wire resetn,
    input wire enable,
    output wire [15:0] dac_data
);
    reg [14:0] phase = 0;
    wire [13:0] ramp = phase[14] ? ~phase[13:0] : phase[13:0];
    wire [15:0] triangle = {2'b00, ramp} - 16'd8192;

    always @(posedge clk) begin
        if (!resetn)
            phase <= 0;
        else
            phase <= phase + 1'b1;
    end

    assign dac_data = enable ? triangle : 16'd0;
endmodule
