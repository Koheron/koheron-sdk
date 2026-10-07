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
    reg [2:0] state = 0;
    reg [7:0] commits=0, writes=0;
    integer k;
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
                    // Decode before driving the replicated programming registers.
                    // This slow handshake never lies on the sample feedback path.
                    rejected <= (|pending[30:10]) ||
                                (!pending[9] && pending[5] == active_banks[target]);
                    for(k=0;k<8;k=k+1) begin
                        commits[k] <= !(|pending[30:10]) && pending[9] && target==k;
                        writes[k] <= !(|pending[30:10]) && !pending[9] && target==k &&
                                     pending[5]!=active_banks[k];
                    end
                    state <= 2;
                end
                2: begin
                    for(k=0;k<8;k=k+1) if(commits[k]) begin
                        active_banks[k] <= pending[5];
                        coefficients[64*k +: 64] <= data;
                    end
                    if(|writes[3:0])
                        command0 <= {1'b0, pending[5:4], pending[3:0], pending[7:6]};
                    if(|writes[7:4])
                        command1 <= {1'b0, pending[5:4], pending[3:0], pending[7:6]};
                    state <= 3;
                end
                3: begin
                    // Address and payload precede the strobe. The RAM samples
                    // the strobe next clock, with at least two setup clocks.
                    if(|writes[3:0]) command0[8]<=1;
                    if(|writes[7:4]) command1[8]<=1;
                    state<=4;
                end
                4: begin
                    // The RAM samples the previous cycle's write command here.
                    command0 <= 0;
                    command1 <= 0;
                    state <= 5;
                end
                5: begin
                    ack <= pending | (rejected ? 32'h40000000 : 0);
                    state <= 0;
                end
            endcase
        end
    end
endmodule
