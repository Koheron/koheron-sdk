module reset_pulser #(
    parameter integer RESET_LEN = 8   // number of clk cycles reset is low
)(
    input  wire clk,
    input  wire global_aresetn,  // active-low
    input  wire pulse_req,       // 1-cycle pulse
    output reg  local_aresetn    // active-low
);

    localparam integer CW = $clog2(RESET_LEN + 1);
    reg [CW-1:0] cnt;

    always @(posedge clk) begin
        if (!global_aresetn) begin
            cnt <= 0;
            local_aresetn <= 1'b0;
        end else if (pulse_req) begin
            cnt <= RESET_LEN[CW-1:0];
            local_aresetn <= 1'b0;
        end else if (cnt != 0) begin
            cnt <= cnt - 1'b1;
            local_aresetn <= 1'b0;
        end else begin
            local_aresetn <= 1'b1;
        end
    end

endmodule