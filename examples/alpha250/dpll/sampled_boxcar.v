`timescale 1 ns / 1 ps

// Four-tap moving average with two arithmetic stages. History advances only
// for valid samples, independently of the processing-clock/sample-rate ratio.
module sampled_boxcar #(
    parameter integer WIDTH=16
) (
    input wire clk, resetn, valid_in,
    input wire signed [WIDTH-1:0] din,
    output wire signed [WIDTH-1:0] dout,
    output reg valid_out=0
);
    reg signed [WIDTH-1:0] history0=0, history1=0, history2=0;
    reg signed [WIDTH:0] sum0=0, sum1=0;
    reg signed [WIDTH+1:0] sum=0;
    reg partial_valid=0;
    always @(posedge clk) begin
        if (!resetn) begin
            history0 <= 0;
            history1 <= 0;
            history2 <= 0;
            sum0 <= 0;
            sum1 <= 0;
            sum <= 0;
            partial_valid <= 0;
            valid_out <= 0;
        end else begin
            partial_valid <= valid_in;
            valid_out <= partial_valid;
            if (valid_in) begin
                history0 <= din;
                history1 <= history0;
                history2 <= history1;
                sum0 <= $signed({din[WIDTH-1],din}) + $signed({history1[WIDTH-1],history1});
                sum1 <= $signed({history0[WIDTH-1],history0}) + $signed({history2[WIDTH-1],history2});
            end
            if (partial_valid)
                sum <= $signed({sum0[WIDTH],sum0}) + $signed({sum1[WIDTH],sum1});
        end
    end
    assign dout = sum[WIDTH+1:2];
endmodule
