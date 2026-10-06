`timescale 1 ns / 1 ps

// Slow, acknowledged programming port in the ADC clock domain. Software holds
// command and data until ack matches command (bit 30 reports rejection).
// [31] request toggle, [9] commit, [8] loop, [7:6] gain, [5] bank,
// [4] signed table, [3:0] address. All other request bits must be zero.
// A commit carries the signed Q*.11 coefficient in cfg_data for readback.
module gain_programmer (
    input wire clk,
    input wire resetn,
    input wire [31:0] cfg_command,
    input wire [63:0] cfg_data,
    output reg [31:0] ack = 0,
    output reg [7:0] active_banks = 0,
    output reg [511:0] coefficients = 0,
    output reg [8:0] command0 = 0,
    output reg [8:0] command1 = 0,
    output reg [63:0] data = 0
);
    reg [1:0] state = 0;
    reg [31:0] pending = 0;
    reg rejected = 0;
    wire [2:0] target = pending[8:6];
    always @(posedge clk) begin
        if (!resetn) begin
            // RAM and bank contents survive a peripheral reset together.
            // Only a new FPGA configuration initializes the gain tables.
            ack <= 0;
            pending <= 0;
            command0 <= 0;
            command1 <= 0;
            state <= 0;
        end else begin
            case (state)
                0: if (cfg_command[31] != ack[31]) begin
                    pending <= cfg_command;
                    data <= cfg_data;
                    state <= 1;
                end
                1: begin
                    rejected <= (|pending[30:10]) ||
                                (!pending[9] && pending[5] == active_banks[target]);
                    if (!(|pending[30:10])) begin
                        if (pending[9]) begin
                            active_banks[target] <= pending[5];
                            coefficients[64*target +: 64] <= data;
                        end else if (pending[5] != active_banks[target]) begin
                            if (pending[8])
                                command1 <= {1'b1, pending[5:4], pending[3:0], pending[7:6]};
                            else
                                command0 <= {1'b1, pending[5:4], pending[3:0], pending[7:6]};
                        end
                    end
                    state <= 2;
                end
                2: begin
                    // The RAM samples the previous cycle's write command here.
                    command0 <= 0;
                    command1 <= 0;
                    state <= 3;
                end
                3: begin
                    ack <= pending | (rejected ? 32'h40000000 : 0);
                    state <= 0;
                end
            endcase
        end
    end
endmodule
