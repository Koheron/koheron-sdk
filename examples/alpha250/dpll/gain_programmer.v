`timescale 1 ns / 1 ps

// Decode software requests on program_clk (143 MHz). Registered handshakes
// carry coherent payloads to/from clk (250 MHz); RAM writes, bank switches,
// coefficient readback and the final acknowledgement belong to clk.
// [31] request toggle, [9] commit, [8] loop, [7:6] gain, [5] bank,
// [4] signed table, [3:0] address. All other request bits must be zero.
module gain_programmer (
    input wire clk,
    input wire resetn,
    input wire program_clk,
    input wire [31:0] cfg_command,
    input wire [63:0] cfg_data,
    output reg [31:0] ack = 0,
    output reg [7:0] active_banks = 0,
    output reg [511:0] coefficients = 0,
    (* max_fanout=16 *) output reg [8:0] command0 = 0,
    (* max_fanout=16 *) output reg [8:0] command1 = 0,
    output reg [63:0] data = 0
);
    localparam F_DRAIN=0, F_IDLE=1, F_WAIT=2, F_APPLIED=3, F_RETURN=4,
               F_STROBE=5, F_WRITE=6, F_ACCEPT=7, F_SETUP=8;
    localparam S_DRAIN=0, S_IDLE=1, S_WAIT=2, S_RETURN=3;
    reg [3:0] fast_state=F_DRAIN;
    reg [1:0] slow_state=S_DRAIN;
    reg [5:0] fast_flush=0, slow_flush=0;
    reg [95:0] request_held=0;
    reg [106:0] response_held=0;
    reg request_send=0, response_send=0;
    reg request_ack=0, response_ack=0;
    reg [31:0] pending_ack=0;
    reg rejected=0, commit_requested=0, loop_requested=0;
    reg [7:0] commit_enable=0;
    integer k;
    wire request_received, response_received;
    wire request_valid, response_valid;
    wire [95:0] request_word;
    wire [106:0] response_word;
    wire program_resetn;
    wire program_ready;

    xpm_cdc_async_rst #(.DEST_SYNC_FF(3), .INIT_SYNC_FF(1), .RST_ACTIVE_HIGH(0)) reset_sync (
        .src_arst(resetn), .dest_clk(program_clk), .dest_arst(program_resetn)
    );
    xpm_cdc_single #(.DEST_SYNC_FF(3), .INIT_SYNC_FF(1), .SRC_INPUT_REG(1)) ready_sync (
        .src_clk(program_clk), .src_in(program_resetn && slow_state!=S_DRAIN),
        .dest_clk(clk), .dest_out(program_ready)
    );

    // XPM handshakes have no peripheral reset. Cancel sends while draining,
    // acknowledge stale arrivals without executing them, and wait 64 clocks
    // in each domain plus an idle handshake before accepting a new request.
    // Preserve held payloads and committed gains throughout a reset.
    xpm_cdc_handshake #(
        .WIDTH(96), .DEST_EXT_HSK(1), .DEST_SYNC_FF(3), .SRC_SYNC_FF(3), .INIT_SYNC_FF(1)
    ) request_cdc (
        .src_clk(clk), .src_in(request_held), .src_send(request_send), .src_rcv(request_received),
        .dest_clk(program_clk), .dest_out(request_word), .dest_req(request_valid), .dest_ack(request_ack)
    );
    xpm_cdc_handshake #(
        .WIDTH(107), .DEST_EXT_HSK(1), .DEST_SYNC_FF(3), .SRC_SYNC_FF(3), .INIT_SYNC_FF(1)
    ) response_cdc (
        .src_clk(program_clk), .src_in(response_held), .src_send(response_send), .src_rcv(response_received),
        .dest_clk(clk), .dest_out(response_word), .dest_req(response_valid), .dest_ack(response_ack)
    );

    always @(posedge program_clk) begin
        if (!program_resetn) begin
            slow_state<=S_DRAIN;
            slow_flush<=0;
            response_send<=0;
            request_ack<=request_valid;
        end else begin
            case (slow_state)
                S_DRAIN: begin
                    request_ack<=request_valid;
                    if (slow_flush!=63) slow_flush<=slow_flush+1'b1;
                    else if (!request_valid && !response_received) slow_state<=S_IDLE;
                end
                S_IDLE: if (request_valid) begin
                    // Reserved-bit validation and table address/control decode
                    // have a 143 MHz budget. The 96-bit request remains intact
                    // for exact acknowledgement and atomic coefficient readback.
                    response_held<={(|request_word[30:10]), request_word[9],
                                    1'b1, request_word[5:4], request_word[3:0], request_word[7:6], request_word};
                    response_send<=1;
                    request_ack<=1;
                    slow_state<=S_WAIT;
                end
                S_WAIT: if (response_received) begin
                    response_send<=0;
                    slow_state<=S_RETURN;
                end
                S_RETURN: if (!response_received && !request_valid) begin
                    request_ack<=0;
                    slow_state<=S_IDLE;
                end
                default: slow_state<=S_DRAIN;
            endcase
        end
    end

    wire [2:0] target=response_word[8:6];
    wire reject_request=response_word[106] ||
        (!response_word[105] && response_word[5]==active_banks[target]);
    // Decode commit acceptance on the existing F_WAIT edge. The following
    // F_ACCEPT edge only gates each bank's wide enable with resetn; the FSM,
    // rejection and target decode no longer sit on the coefficient hold path.
    // Reset must still cancel a commit on its apply edge while preserving
    // every previously committed bank and coefficient.
    always @(posedge clk) begin
        commit_enable<=0;
        if (resetn && fast_state==F_WAIT && response_valid &&
            !reject_request && response_word[105])
            commit_enable<=8'b1<<target;
        for(k=0;k<8;k=k+1) if (resetn && commit_enable[k]) begin
            active_banks[k]<=response_word[5];
            coefficients[64*k +: 64]<=response_word[95:32];
        end
    end

    always @(posedge clk) begin
        // One fast-clock write pulse regardless of the slow-clock phase.
        command0[8]<=0;
        command1[8]<=0;
        if (!resetn) begin
            command0<=0;
            command1<=0;
            ack<=0;
            fast_state<=F_DRAIN;
            fast_flush<=0;
            request_send<=0;
            response_ack<=response_valid;
        end else begin
            if (request_send && request_received) request_send<=0;
            case (fast_state)
                F_DRAIN: begin
                    response_ack<=response_valid;
                    if (fast_flush!=63) fast_flush<=fast_flush+1'b1;
                    else if (!request_received && !response_valid) fast_state<=F_IDLE;
                end
                F_IDLE: if (program_ready && cfg_command[31]!=ack[31] && !request_received && !request_send && !response_valid) begin
                    request_held<={cfg_data,cfg_command};
                    request_send<=1;
                    fast_state<=F_WAIT;
                end
                F_WAIT: if (response_valid) begin
                    pending_ack<=response_word[31:0] | (reject_request ? 32'h40000000 : 32'd0);
                    // Register acceptance before driving the RAM controls.
                    // The handshake holds response_word until F_APPLIED.
                    rejected<=reject_request;
                    commit_requested<=response_word[105];
                    loop_requested<=response_word[8];
                    fast_state<=F_ACCEPT;
                end
                F_ACCEPT: begin
                    if (!rejected && !commit_requested) begin
                        data<=response_word[95:32];
                        // Hold payload/address before asserting the write
                        // strobe, providing three complete setup clocks.
                        if (loop_requested) command1<={1'b0,response_word[103:96]};
                        else command0<={1'b0,response_word[103:96]};
                    end
                    fast_state<=(!rejected && !commit_requested) ? F_SETUP : F_APPLIED;
                end
                F_SETUP: fast_state<=F_STROBE;
                F_STROBE: begin
                    if (loop_requested) command1[8]<=1;
                    else command0[8]<=1;
                    fast_state<=F_WRITE;
                end
                F_WRITE: begin
                    // Table write ports consume the prior cycle's strobe.
                    fast_state<=F_APPLIED;
                end
                F_APPLIED: begin
                    // Direct and retimed RAM writes finish by this edge. Commits
                    // apply bank and coefficient atomically in F_ACCEPT.
                    ack<=pending_ack;
                    response_ack<=1;
                    fast_state<=F_RETURN;
                end
                F_RETURN: if (!response_valid) begin
                    response_ack<=0;
                    fast_state<=F_IDLE;
                end
                default: fast_state<=F_DRAIN;
            endcase
        end
    end
endmodule
