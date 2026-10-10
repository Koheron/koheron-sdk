`timescale 1ns/1ps
module test_gain_programmer_reset_tb;
    reg clk=0, program_clk=0, resetn=0;
    always #2 clk=~clk;
    initial begin #1.3; forever #3.5 program_clk=~program_clk; end
    reg [31:0] command=0;
    reg [63:0] payload=0;
    wire [31:0] ack;
    wire [7:0] banks;
    wire [511:0] coefficients;
    wire [8:0] write0,write1;
    wire [63:0] data;
    gain_programmer dut(clk,resetn,program_clk,command,payload,ack,banks,coefficients,write0,write1,data);
    integer writes=0;
    reg previous_write=0;
    always @(posedge clk) begin
        if(write0[8] && write1[8]) $fatal(1,"Two loop writes");
        if(write0[8] || write1[8]) begin
            if(previous_write) $fatal(1,"Repeated write");
            writes=writes+1;
        end
        previous_write=write0[8] || write1[8];
    end
    task transact(input [31:0] flags,input [63:0] value,input rejected);
        integer timeout;
        begin
            @(negedge clk); payload=value; command={~ack[31],31'b0}|flags;
            timeout=0;
            while((ack & 32'hbfffffff)!=command) begin
                @(negedge clk); timeout=timeout+1;
                if(timeout>300) $fatal(1,"Reset recovery acknowledgement timeout");
            end
            if(ack[30]!==rejected) $fatal(1,"Wrong rejection status");
        end
    endtask
    integer delay_cycles,before_reset,target,timeout;
    reg [31:0] commit_command;
    reg [63:0] commit_value;
    reg [7:0] saved_banks;
    reg [511:0] saved_coefficients;
    initial begin
        repeat(8) @(negedge clk); resetn=1;
        repeat(200) @(negedge clk);
        // Preserve actual committed gains while resetting at every point of
        // request, decode, response, apply and handshake rearm.
        transact(32'h220,64'hf123456789abcdef,0);
        for(delay_cycles=0;delay_cycles<65;delay_cycles=delay_cycles+1) begin
            @(negedge clk); command={~ack[31],31'b0}|32'h120; payload=delay_cycles;
            repeat(delay_cycles) @(negedge clk);
            resetn=0;command=0;
            saved_banks=banks;saved_coefficients=coefficients;
            // A write pulse already issued before reset is consumed on the
            // first reset edge; no subsequent pulse may escape the drain.
            @(negedge clk);before_reset=writes;
            repeat(4) @(negedge clk);resetn=1;
            repeat(200) @(negedge clk);
            if(writes!=before_reset || ack!=0 || banks!==saved_banks || coefficients!==saved_coefficients)
                $fatal(1,"Stale transaction applied across reset at delay=%0d",delay_cycles);
            before_reset=writes;
            transact(32'h120,64'h8123456789abcdef+delay_cycles,0);
            if(writes!=before_reset+1 || write1[8] || data!==64'h8123456789abcdef+delay_cycles)
                $fatal(1,"Recovery write lost or corrupted");
            // Request reserved bits and writes to a committed active bank
            // must produce acknowledgements without RAM writes.
            transact(32'h1020,64'hdeadbeefdeadbeef,1);
            transact(32'h20,64'hdeadbeefdeadbeef,1);
            if(writes!=before_reset+1) $fatal(1,"Rejected write executed");
        end
        // Sweep reset across commits as well as writes. In particular, reset
        // on the apply edge must cancel a decoded commit without clearing any
        // previously committed bank or coefficient. Exercise every destination.
        for(target=0;target<8;target=target+1) begin
            for(delay_cycles=0;delay_cycles<65;delay_cycles=delay_cycles+1) begin
                commit_command=32'h200 | (target<<6) | ((!banks[target])<<5);
                commit_value=64'hfedcba9876543210 ^ (target<<16) ^ delay_cycles;
                @(negedge clk);
                command={~ack[31],31'b0}|commit_command;payload=commit_value;
                repeat(delay_cycles) @(negedge clk);
                resetn=0;command=0;
                saved_banks=banks;saved_coefficients=coefficients;before_reset=writes;
                repeat(5) @(negedge clk);resetn=1;
                repeat(200) @(negedge clk);
                if(writes!=before_reset || ack!=0 || banks!==saved_banks || coefficients!==saved_coefficients)
                    $fatal(1,"Commit escaped reset: target=%0d delay=%0d",target,delay_cycles);
                // Successful recovery commits must change only the selected
                // bank and its coefficient, on the same edge.
                transact(commit_command,commit_value,0);
                saved_banks[target]=commit_command[5];
                saved_coefficients[64*target +: 64]=commit_value;
                if(banks!==saved_banks || coefficients!==saved_coefficients || writes!=before_reset)
                    $fatal(1,"Commit recovery corrupted a bank: target=%0d",target);
                transact(commit_command | 32'h1000,~commit_value,1);
                if(banks!==saved_banks || coefficients!==saved_coefficients || writes!=before_reset)
                    $fatal(1,"Rejected commit changed gains");
            end
        end
        // Delays alone need not hit a particular apply edge: the two clock
        // domains and handshake rearm move it between transactions. Interrupt
        // that edge explicitly for every target, with distinct new coefficients.
        for(target=0;target<8;target=target+1) begin
            commit_command=32'h200 | (target<<6) | ((!banks[target])<<5);
            commit_value=~coefficients[64*target +: 64];
            @(negedge clk);
            command={~ack[31],31'b0}|commit_command;payload=commit_value;
            timeout=0;
            while(dut.fast_state!=dut.F_ACCEPT) begin
                @(negedge clk);timeout=timeout+1;
                if(timeout>300) $fatal(1,"Commit never reached its apply edge");
            end
            saved_banks=banks;saved_coefficients=coefficients;before_reset=writes;
            resetn=0;command=0;
            repeat(5) @(negedge clk);resetn=1;
            repeat(200) @(negedge clk);
            if(writes!=before_reset || ack!=0 || banks!==saved_banks || coefficients!==saved_coefficients)
                $fatal(1,"Commit applied on reset edge: target=%0d",target);
            transact(commit_command,commit_value,0);
            saved_banks[target]=commit_command[5];
            saved_coefficients[64*target +: 64]=commit_value;
            if(banks!==saved_banks || coefficients!==saved_coefficients || writes!=before_reset)
                $fatal(1,"Apply-edge reset prevented recovery: target=%0d",target);
        end
        $display("Gain programmer reset checks passed: 65 write reset positions, 520 commit reset positions and eight exact commit-edge resets, coherent readback and successful reprogramming");
        $finish;
    end
    initial begin #2000000; $fatal(1,"Watchdog"); end
endmodule
