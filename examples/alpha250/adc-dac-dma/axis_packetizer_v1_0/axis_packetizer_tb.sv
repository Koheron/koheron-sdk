`timescale 1ns / 1ps

module axis_packetizer_tb;
    reg aclk = 0;
    always #5 aclk = ~aclk;

    reg aresetn = 0;
    reg trig = 0;
    reg [31:0] pkt_count = 2;
    reg m_axis_tready = 1;
    reg [63:0] s_axis_tdata = 0;
    reg [63:0] source_time = 0;
    wire s_axis_tready;
    wire m_axis_tvalid;
    wire m_axis_tlast;
    wire [63:0] m_axis_tdata;
    wire [7:0] m_axis_tkeep;
    integer received = 0;
    integer first_sample = -1;
    integer previous_sample = -1;
    integer expected_beats = 8;

    // A one-word model of the width converter: input samples continue to
    // advance, but its output cannot refresh while TREADY is low.
    always @(posedge aclk) begin
        if (!aresetn) begin
            source_time <= 0;
            s_axis_tdata <= 0;
        end else begin
            source_time <= source_time + 1;
            if (s_axis_tready)
                s_axis_tdata <= source_time;
        end
    end

    axis_packetizer #(.TDATA_WIDTH(64), .PKT_LENGTH(4)) dut (
        .aclk(aclk), .aresetn(aresetn), .trig(trig), .pkt_count(pkt_count),
        .s_axis_tvalid(1'b1), .s_axis_tready(s_axis_tready),
        .s_axis_tdata(s_axis_tdata), .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tready(m_axis_tready), .m_axis_tlast(m_axis_tlast),
        .m_axis_tdata(m_axis_tdata), .m_axis_tkeep(m_axis_tkeep)
    );

    always @(posedge aclk) begin
        if (aresetn && m_axis_tvalid && m_axis_tready) begin
            if (received == 0)
                first_sample = m_axis_tdata;
            else if (m_axis_tdata != previous_sample + 1)
                $fatal(1, "nonconsecutive samples at beat %0d", received);
            if (m_axis_tlast !== ((received % 4) == 3))
                $fatal(1, "wrong TLAST at beat %0d", received);
            if (m_axis_tkeep !== 8'hff)
                $fatal(1, "wrong TKEEP");
            previous_sample = m_axis_tdata;
            received = received + 1;
        end
    end

    initial begin
        repeat (2) @(negedge aclk);
        if (s_axis_tready !== 1'b0)
            $fatal(1, "TREADY asserted during reset");
        repeat (3) @(negedge aclk);
        aresetn = 1;
        repeat (20) @(negedge aclk);
        if (s_axis_tready !== 1'b1)
            $fatal(1, "idle stream is not being drained");
        trig = 1;
        @(negedge aclk);
        trig = 0;
        wait (received == expected_beats);
        @(negedge aclk);
        if (first_sample < 15)
            $fatal(1, "stale first sample: %0d", first_sample);
        if (m_axis_tvalid !== 1'b0 || s_axis_tready !== 1'b1)
            $fatal(1, "packetizer did not return to idle");
        $display("PASS: first sample %0d, %0d beats, TLAST at each packet boundary", first_sample, received);
        $finish;
    end

    initial begin
        #2000;
        $fatal(1, "timed out");
    end
endmodule
