`timescale 1ns/1ps
module test_pna_phase_history_tb;
    reg clk=0, rst=1, acc_on=0;
    always #2 clk=~clk;
    reg signed [15:0] phase_in=0;
    wire signed [16:0] frequency;
    wire signed [63:0] history;
    wire overflow;
    phase_unwrapper #(.DIN_WIDTH(16), .DOUT_WIDTH(64),
        .PIPELINED_OVERFLOW(1), .PIPELINED_HISTORY(1)) dut(
        clk, acc_on, rst, phase_in, frequency, history, overflow);

    // Independent signed arithmetic model of the conventional unwrap and full
    // 64-bit accumulation. Only the history and sticky flag are delayed.
    reg signed [15:0] previous=0;
    reg signed [16:0] difference=0, increment=0;
    reg signed [64:0] next_history;
    reg signed [63:0] expected=0, expected_d=0;
    reg sticky=0, sticky_d=0, sticky_dd=0;
    integer checked=0, seed=19481;
    always @(posedge clk) begin
        expected_d=expected;
        sticky_dd=sticky_d;
        next_history=$signed(expected)+$signed(increment);
        if (rst) begin
            expected=0; expected_d=0;
            sticky=0; sticky_d=0; sticky_dd=0;
        end else begin
            sticky_d=sticky;
            if (acc_on) begin
                expected=next_history[63:0];
                sticky=sticky | (next_history[64]!=next_history[63]);
            end
        end
        if (difference>17'sd8192) increment=difference-17'sd16384;
        else if (difference< -17'sd8192) increment=difference+17'sd16384;
        else increment=difference;
        difference=$signed(phase_in)-$signed(previous);
        previous=phase_in;
        #1;
        if (history!==expected_d || frequency!==increment || overflow!==sticky_dd)
            $fatal(1,"PNA history mismatch cycle=%0d history=%h expected=%h overflow=%b/%b",
                checked,history,expected_d,overflow,sticky_dd);
        checked=checked+1;
    end

    task seed_history(input [63:0] value);
        begin
            rst=1; acc_on=0; phase_in=0;
            repeat (6) @(negedge clk);
            // These boundaries are unreachable in a short phase walk. Seed
            // consistent state, then use the normal datapath to cross them.
            rst=0; acc_on=1;
            dut.phase_out=value;
            dut.split_history.low_state=value[31:0];
            expected=value;
        end
    endtask

    initial begin
        repeat (6) @(negedge clk);
        for (integer boundary=0; boundary<4; boundary=boundary+1) begin
            case (boundary)
                0: seed_history(64'h7fffffffffffffff);
                1: seed_history(64'h8000000000000000);
                2: seed_history(64'h00000000ffffffff);
                3: seed_history(64'hffffffff00000000);
            endcase
            phase_in=boundary%2 ? -16'sd1 : 16'sd1;
            repeat (6) @(negedge clk);
            if (boundary<2 && !overflow) $fatal(1,"Missing signed overflow");
            if (boundary>=2 && overflow) $fatal(1,"Spurious overflow at low-word carry");
            // Disable immediately after new increments: any pending upper-word
            // update must finish, then the delayed complete history must hold.
            phase_in=boundary%2 ? -16'sd8192 : 16'sd8192;
            @(negedge clk);
            acc_on=0;
            repeat (6) @(negedge clk);
        end
        rst=1;
        repeat (6) @(negedge clk);
        rst=0; acc_on=1;
        for (integer k=0; k<50000; k=k+1) begin
            case (k%17)
                0: phase_in=16'sd8192;
                1: phase_in=-16'sd8192;
                2: phase_in=16'sd8193;
                3: phase_in=-16'sd8193;
                4: phase_in=16'sh7fff;
                5: phase_in=16'sh8000;
                default: phase_in=$random(seed);
            endcase
            acc_on=(k%11!=0);
            rst=(k%997==996);
            @(negedge clk);
        end
        $display("PNA phase history checks passed: %0d cycles, signed overflow both directions, low-word carry/borrow, pi ties, full-width inputs, enables, pending updates, reset and exact one-clock history delay",checked);
        $finish;
    end
endmodule
