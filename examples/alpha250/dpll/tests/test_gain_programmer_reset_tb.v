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
        if((|write0) && (|write1)) $fatal(1,"Two loop writes");
        if((|write0) || (|write1)) begin
            if(previous_write) $fatal(1,"Repeated write");
            writes=writes+1;
        end
        previous_write=(|write0) || (|write1);
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
    integer delay_cycles,before_reset;
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
            if(writes!=before_reset+1 || write1!=0 || data!==64'h8123456789abcdef+delay_cycles)
                $fatal(1,"Recovery write lost or corrupted");
            // Request reserved bits and writes to a committed active bank
            // must produce acknowledgements without RAM writes.
            transact(32'h1020,64'hdeadbeefdeadbeef,1);
            transact(32'h20,64'hdeadbeefdeadbeef,1);
            if(writes!=before_reset+1) $fatal(1,"Rejected write executed");
        end
        $display("Gain programmer reset checks passed: 65 reset positions, coherent readback, no stale/repeated writes and successful reprogramming");
        $finish;
    end
    initial begin #1000000; $fatal(1,"Watchdog"); end
endmodule
