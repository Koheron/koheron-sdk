`timescale 1 ns / 1 ps
module test_control_tb;
    localparam WORDS = 17;
    reg clk = 0, resetn = 0;
    reg [15:0] awaddr = 0, araddr = 0;
    reg [31:0] wdata = 0;
    reg [3:0] wstrb = 0;
    reg awvalid = 0, wvalid = 0, arvalid = 0, bready = 0, rready = 0;
    wire [WORDS*32-1:0] ctl_old, ctl_new;
    wire awready_old, awready_new, wready_old, wready_new;
    wire bvalid_old, bvalid_new, arready_old, arready_new, rvalid_old, rvalid_new;
    wire [1:0] bresp_old, bresp_new, rresp_old, rresp_new;
    wire [31:0] rdata_old, rdata_new;
    reg aw_taken = 0, w_taken = 0, ar_taken = 0;
    reg have_aw = 0, have_w = 0, have_ar = 0, last_bvalid = 0, last_rvalid = 0;
    reg [15:0] pending_aw, pending_ar;
    reg [31:0] pending_w, expected_read;
    reg [3:0] pending_strobes;
    reg [31:0] memory [0:WORDS-1];
    integer i, j, word_index, cycle = 0, writes = 0, reads = 0, seed = 95319;
    always #2 clk = ~clk;
    axi_ctl_register #(.CTL_DATA_WIDTH(WORDS*32), .PREDECODE_WRITES(0)) old_dut (
        .aclk(clk),
        .aresetn(resetn),
        .ctl_data(ctl_old),
        .s_axi_awaddr(awaddr),
        .s_axi_awvalid(awvalid),
        .s_axi_awready(awready_old),
        .s_axi_wdata(wdata),
        .s_axi_wstrb(wstrb),
        .s_axi_wvalid(wvalid),
        .s_axi_wready(wready_old),
        .s_axi_bresp(bresp_old),
        .s_axi_bvalid(bvalid_old),
        .s_axi_bready(bready),
        .s_axi_araddr(araddr),
        .s_axi_arvalid(arvalid),
        .s_axi_arready(arready_old),
        .s_axi_rdata(rdata_old),
        .s_axi_rresp(rresp_old),
        .s_axi_rvalid(rvalid_old),
        .s_axi_rready(rready)
    );
    axi_ctl_register #(.CTL_DATA_WIDTH(WORDS*32), .PREDECODE_WRITES(1)) new_dut (
        .aclk(clk),
        .aresetn(resetn),
        .ctl_data(ctl_new),
        .s_axi_awaddr(awaddr),
        .s_axi_awvalid(awvalid),
        .s_axi_awready(awready_new),
        .s_axi_wdata(wdata),
        .s_axi_wstrb(wstrb),
        .s_axi_wvalid(wvalid),
        .s_axi_wready(wready_new),
        .s_axi_bresp(bresp_new),
        .s_axi_bvalid(bvalid_new),
        .s_axi_bready(bready),
        .s_axi_araddr(araddr),
        .s_axi_arvalid(arvalid),
        .s_axi_arready(arready_new),
        .s_axi_rdata(rdata_new),
        .s_axi_rresp(rresp_new),
        .s_axi_rvalid(rvalid_new),
        .s_axi_rready(rready)
    );
    always @(posedge clk) begin
        cycle = cycle + 1;
        aw_taken = resetn && awvalid && awready_old;
        w_taken = resetn && wvalid && wready_old;
        ar_taken = resetn && arvalid && arready_old;
        if (!resetn) begin
            have_aw = 0; have_w = 0; have_ar = 0;
            for (j = 0; j < WORDS; j = j + 1) memory[j] = 0;
        end else begin
            if (aw_taken) begin pending_aw = awaddr; have_aw = 1; end
            if (w_taken) begin pending_w = wdata; pending_strobes = wstrb; have_w = 1; end
            if (ar_taken) begin pending_ar = araddr; have_ar = 1; end
        end
        #1;
        if ({ctl_old, awready_old, wready_old, bvalid_old, bresp_old, arready_old, rvalid_old, rresp_old, rdata_old} !==
            {ctl_new, awready_new, wready_new, bvalid_new, bresp_new, arready_new, rvalid_new, rresp_new, rdata_new})
            $fatal(1, "Control decoder protocol/data/latency mismatch at cycle %0d", cycle);
        if (resetn) begin
            // Reads observe the register state before a simultaneous write.
            if (rvalid_old && !last_rvalid) begin
                if (!have_ar) $fatal(1, "Read response without an accepted address");
                word_index = pending_ar >> 2;
                expected_read = (word_index < WORDS) ? memory[word_index] : 0;
                if (rdata_new !== expected_read || rresp_new !== 0)
                    $fatal(1, "Read scoreboard mismatch at cycle %0d", cycle);
                have_ar = 0; reads = reads + 1;
            end
            if (bvalid_old && !last_bvalid) begin
                if (!have_aw || !have_w || bresp_new !== 0)
                    $fatal(1, "Write response without accepted AW and W");
                word_index = pending_aw >> 2;
                if (word_index < WORDS)
                    for (j = 0; j < 4; j = j + 1)
                        if (pending_strobes[j]) memory[word_index][j*8 +: 8] = pending_w[j*8 +: 8];
                have_aw = 0; have_w = 0; writes = writes + 1;
            end
        end
        for (j = 0; j < WORDS; j = j + 1)
            if (ctl_new[j*32 +: 32] !== memory[j])
                $fatal(1, "Write scoreboard mismatch at cycle %0d word %0d", cycle, j);
        last_bvalid = bvalid_old; last_rvalid = rvalid_old;
    end
    initial begin
        repeat (4) @(negedge clk);
        for (i = 0; i < 10000; i = i + 1) begin
            @(negedge clk);
            resetn = (i % 701 >= 2);
            if (!resetn) begin awvalid = 0; wvalid = 0; arvalid = 0; end
            else begin
                if (aw_taken) awvalid = 0;
                if (w_taken) wvalid = 0;
                if (ar_taken) arvalid = 0;
                if (!awvalid && ($random(seed) & 3) != 0) begin
                    awvalid = 1;
                    awaddr = (i % 127 == 0) ? 16'hffff : ($random(seed) & 127);
                end
                if (!wvalid && ($random(seed) & 3) != 0) begin
                    wvalid = 1; wdata = $random(seed); wstrb = $random(seed);
                end
                if (!arvalid && ($random(seed) & 3) != 0) begin
                    arvalid = 1;
                    araddr = (i % 113 == 0) ? 16'hffff : ($random(seed) & 127);
                end
            end
            bready = (i % 97 >= 8) && (($random(seed) & 3) != 0);
            rready = (i % 89 >= 8) && (($random(seed) & 3) != 0);
        end
        @(negedge clk); awvalid = 0; wvalid = 0; arvalid = 0; bready = 1; rready = 1;
        repeat (20) @(negedge clk);
        if (writes < 1000 || reads < 1000) $fatal(1, "Insufficient AXI coverage");
        $display("Control checks passed: %0d cycles, %0d writes, %0d reads, exact data and response latency", cycle, writes, reads);
        $finish;
    end
endmodule
