`timescale 1ns/1ps
module test_monitor_cdc_tb;
    reg fast_clk=0, slow_clk=0;
    always #2 fast_clk=~fast_clk;
    always #3.4965035 slow_clk=~slow_clk;
    reg resetn_in=0;
    reg [5:0] metadata_in=0;
    reg [31:0] packet_in=0;
    wire resetn;
    wire [5:0] metadata;
    wire [31:0] packet_status;
    reg [15:0] rate_in=20;
    wire [15:0] rate;
    phase_stream_cdc dut(.clk(slow_clk), .status_clk(fast_clk), .resetn_in(resetn_in),
        .metadata_in(metadata_in), .packet_in(packet_in), .rate_in(rate_in),
        .resetn(resetn), .metadata(metadata), .rate(rate), .packet_status(packet_status));
    integer checks=0;
    reg [31:0] last=0;
    // A valid status is a coherent counter plus its bitwise complement.
    always @(posedge fast_clk) begin
        #0.1;
        if (packet_status != 0 && packet_status[15:0] !== ~packet_status[31:16])
            $fatal(1, "Torn packet status %h", packet_status);
        if (packet_status != last) begin checks=checks+1; last=packet_status; end
    end
    integer i;
    initial begin
        repeat(10) @(negedge slow_clk);
        if (resetn !== 0) $fatal(1,"Reset assertion");
        metadata_in=6'b001000; resetn_in=1;
        repeat(6) @(negedge slow_clk);
        if (!resetn || metadata !== metadata_in || rate !== rate_in) $fatal(1,"Reset/precision/rate synchronization");
        for (i=1;i<=100;i=i+1) begin
            packet_in={16'(i), ~16'(i)};
            // Exercise updates even while the previous word is in flight.
            repeat(7) @(negedge slow_clk);
        end
        metadata_in=6'b111000;
        repeat(6) @(negedge slow_clk);
        if (metadata !== metadata_in) $fatal(1,"Sticky error synchronization");
        resetn_in=0;
        #0.1;
        if (resetn !== 0) $fatal(1,"Asynchronous reset assertion");
        repeat(40) @(negedge slow_clk);
        if (packet_status !== packet_in || checks < 20) $fatal(1,"Status did not progress");
        $display("PASS: monitor 250/143 MHz reset, metadata and coherent status (%0d words)",checks);
        $finish;
    end
    initial begin #100000; $fatal(1,"Timeout"); end
endmodule
