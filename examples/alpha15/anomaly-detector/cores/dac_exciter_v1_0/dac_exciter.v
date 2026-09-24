`timescale 1ns/1ps
// Small triangle and a one-microsecond pulse, both generated on the FPGA.
module dac_exciter (
    input wire clk, input wire resetn, input wire inject_toggle,
    output reg [15:0] dac_data = 0
);
    reg [14:0] phase = 0;
    reg [5:0] phase_div = 0;
    reg inject_meta = 0, inject_sync = 0;
    reg seen = 0;
    reg [7:0] pulse_left = 0;
    wire [13:0] ramp = phase[14] ? ~phase[13:0] : phase[13:0];
    wire signed [16:0] triangle = $signed({3'b000,ramp}) - 17'sd8192;
    wire signed [16:0] driven = triangle + (pulse_left != 0 ? 17'sd8192 : 17'sd0);
    always @(posedge clk) begin
        if (!resetn) begin
            phase <= 0; phase_div <= 0; inject_meta <= 0;
            inject_sync <= 0; seen <= 0; pulse_left <= 0; dac_data <= 0;
        end else begin
            phase_div <= phase_div + 1'b1;
            if (phase_div == 6'd63) phase <= phase + 1'b1;
            inject_meta <= inject_toggle;
            inject_sync <= inject_meta;
            dac_data <= driven[15:0];
            if (inject_sync != seen) begin
                seen <= inject_sync;
                pulse_left <= 8'd240;
            end else if (pulse_left != 0)
                pulse_left <= pulse_left - 1'b1;
        end
    end
endmodule
