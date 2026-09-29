`timescale 1 ns / 1 ps

// DRP mailbox on the independent PS clock, usable while the MMCM is reset.
// ctl: reset[31], request toggle[24], write[23], address[22:16], data[15:0].
// sts: completion toggle[24], synchronized locked[16], read data[15:0].
module mmcm_drp (
    input wire aclk,
    input wire [31:0] ctl,
    output wire [31:0] sts,
    output wire reset,
    output reg den = 0,
    output reg dwe = 0,
    output reg [6:0] daddr = 0,
    output reg [15:0] di = 0,
    input wire drdy,
    input wire [15:0] dout,
    input wire locked
);
    reg initialized = 0;
    reg busy = 0;
    reg request = 0;
    reg completed = 0;
    reg [15:0] result = 0;
    (* ASYNC_REG = "TRUE" *) reg [1:0] locked_sync = 0;
    // The external clock may still be at the previous instrument's rate.
    // Stay reset from configuration until software explicitly initializes us.
    assign reset = ctl[31] | !initialized;
    assign sts = {7'b0, completed, 7'b0, locked_sync[1], result};
    always @(posedge aclk) begin
        if (ctl[31]) initialized <= 1;
        locked_sync <= {locked_sync[0], locked};
        den <= 0;
        if (!busy && ctl[24] != completed) begin
            request <= ctl[24];
            dwe <= ctl[23];
            daddr <= ctl[22:16];
            di <= ctl[15:0];
            den <= 1;
            busy <= 1;
        end
        if (busy && drdy) begin
            result <= dout;
            completed <= request;
            busy <= 0;
        end
    end
endmodule
