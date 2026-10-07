`timescale 1 ns / 1 ps
// Independent feedback enable and monitor epoch, sharing accurate extraction.
module accurate_phase_consumers(
    input wire clk, resetn, acc_on,
    input wire signed [24:0] frequency,
    input wire signed [63:0] phase,
    input wire [7:0] random_round,
    output reg signed [39:0] feedback_phase=0,
    output reg signed [63:0] monitor_phase=0
);
    wire signed [64:0] rounded=$signed({phase[63],phase})+$signed({57'b0,random_round});
    // Bound the local carry chains without adding a feedback register.
    wire [39:0] next_feedback;
    phase_unwrapper_adder #(.WIDTH(40),.BLOCK(8)) feedback_add(
        feedback_phase,{{15{frequency[24]}},frequency},1'b0,next_feedback);
    always @(posedge clk) begin
        if(!resetn) begin feedback_phase<=0; monitor_phase<=0; end
        else begin
            if(acc_on) feedback_phase<=next_feedback;
            monitor_phase<=rounded>>>8;
        end
    end
endmodule

// A monitor reset changes its origin, never the shared phase history.
module monitor_phase_origin(
    input wire clk, resetn,
    input wire signed [63:0] phase,
    output reg signed [63:0] relative_phase=0,
    output reg overflow=0
);
    reg signed [63:0] origin=0;
    wire signed [64:0] difference=$signed({phase[63],phase})-$signed({origin[63],origin});
    always @(posedge clk) begin
        if(!resetn) begin origin<=phase; relative_phase<=0; overflow<=0; end
        else begin
            relative_phase<=difference[63:0];
            overflow<=overflow | (difference[64]!=difference[63]);
        end
    end
endmodule
