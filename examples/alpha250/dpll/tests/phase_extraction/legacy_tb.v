`timescale 1 ns / 1 ps
module phase_extraction_legacy_test;
    reg clk = 0, resetn = 0, valid = 0;
    reg signed [23:0] i_in = 0, q_in = 0;
    wire valid_fast, valid_single, valid_baseline, valid_compact;
    wire signed [15:0] fast, single, baseline, compact;
    phase_extractor #(.RESIDUAL_CORRECTION(0), .PHASE_WIDTH(16), .ITERATIONS(16), .ROTATIONS_PER_CLOCK(2)) dut(clk,resetn,valid,i_in,q_in,valid_fast,fast);
    phase_extractor #(.RESIDUAL_CORRECTION(0), .PHASE_WIDTH(16), .ITERATIONS(16), .ROTATIONS_PER_CLOCK(1), .FUSE_ROUND(0), .COMPACT_PREP(0)) reference_pipeline(clk,resetn,valid,i_in,q_in,valid_single,single);
    phase_extractor #(.RESIDUAL_CORRECTION(0), .PHASE_WIDTH(16), .ITERATIONS(16), .FUSE_ROUND(0), .COMPACT_PREP(0)) baseline_pipeline(clk,resetn,valid,i_in,q_in,valid_baseline,baseline);
    phase_extractor #(.RESIDUAL_CORRECTION(0), .PHASE_WIDTH(16), .ITERATIONS(16), .COMPACT_PREP(1)) compact_pipeline(clk,resetn,valid,i_in,q_in,valid_compact,compact);
    always #2 clk = ~clk;
    integer expected = 0, expected_pipe [0:19];
    real exact = 0, exact_pipe [0:19];
    integer compact_history [0:5];
    reg [19:0] valid_pipe = 0;
    integer k, dx, dy, ev, vv, rr, file, status, checked = 0, cycles = 0;
    integer error, max_error = 0;
    integer abs_i, abs_q, magnitude, expected_shift, b;
    real sum_squared_error = 0;
    real exact_error, max_exact_error = 0, sum_exact_squared = 0;
    real raw_error, max_raw_error = 0, sum_raw_squared = 0;
    integer raw_checked = 0;
    reg [2047:0] vector_path;
    always @(posedge clk) begin
        for (k=19;k>0;k=k-1) begin
            expected_pipe[k] = expected_pipe[k-1];
            exact_pipe[k] = exact_pipe[k-1];
        end
        expected_pipe[0] = expected;
        exact_pipe[0] = exact;
        valid_pipe = resetn ? {valid_pipe[18:0],valid} : 0;
        for (k=5;k>0;k=k-1) compact_history[k] = compact_history[k-1];
        compact_history[0] = compact;
        cycles = cycles + 1;
        abs_i = $signed(i_in) < 0 ? -$signed(i_in) : $signed(i_in);
        abs_q = $signed(q_in) < 0 ? -$signed(q_in) : $signed(q_in);
        magnitude = abs_i | abs_q;
        expected_shift = 23;
        for (b=0;b<24;b=b+1) if ((magnitude >> b) & 1) expected_shift = 23-b;
        #0.1;
        if (compact_pipeline.compact_prep.shift !== expected_shift[4:0])
            $fatal(1,"Leading-bit detection mismatch at cycle %0d: IQ=(%0d,%0d)",cycles,$signed(i_in),$signed(q_in));
        if (valid_fast !== valid_pipe[14] || valid_compact !== valid_pipe[13] || valid_baseline !== valid_pipe[15] || valid_single !== valid_pipe[19])
            $fatal(1,"Valid/latency mismatch at cycle %0d",cycles);
        // Baseline's final angle register precedes its rounding register by
        // one clock. Its 22-bit circular slice has pi = 2^21.
        if (valid_pipe[14]) begin
            raw_error = baseline_pipeline.zeros[12] ? 0.0 : $signed(baseline_pipeline.z[16][21:0]) / 256.0;
            raw_error = raw_error - exact_pipe[14];
            if (raw_error > 8192) raw_error = raw_error - 16384;
            if (raw_error < -8192) raw_error = raw_error + 16384;
            if (raw_error < 0) raw_error = -raw_error;
            if (raw_error*383.4951969714103 > 40.0)
                $fatal(1,"Pre-rounding phase error exceeds 40 urad at cycle %0d",cycles);
            if (raw_error > max_raw_error) max_raw_error = raw_error;
            sum_raw_squared = sum_raw_squared + raw_error*raw_error;
            raw_checked = raw_checked + 1;
        end
        if (valid_fast) begin
            if ((^fast) === 1'bx) $fatal(1,"Unknown phase output");
            error = $signed(fast) - expected_pipe[14];
            if (error > 8192) error = error - 16384;
            if (error < -8192) error = error + 16384;
            if (error < 0) error = -error;
            if (error > 1) $fatal(1,"Angular error %0d LSB at cycle %0d (actual=%0d expected=%0d)",error,cycles,$signed(fast),expected_pipe[14]);
            if (error > max_error) max_error = error;
            sum_squared_error = sum_squared_error + error*error;
            exact_error = $signed(fast) - exact_pipe[14];
            if (exact_error > 8192) exact_error = exact_error - 16384;
            if (exact_error < -8192) exact_error = exact_error + 16384;
            if (exact_error < 0) exact_error = -exact_error;
            if (exact_error > 0.6) $fatal(1,"Continuous atan2 error %f LSB at cycle %0d",exact_error,cycles);
            if (exact_error > max_exact_error) max_exact_error = exact_error;
            sum_exact_squared = sum_exact_squared + exact_error*exact_error;
            checked = checked + 1;
        end
        if (valid_fast && $signed(fast) !== compact_history[0])
            $fatal(1,"Compact preparation changed arithmetic at cycle %0d",cycles);
        if (valid_baseline && $signed(baseline) !== compact_history[1])
            $fatal(1,"Fused rounding changed arithmetic at cycle %0d",cycles);
        if (valid_single && $signed(single) !== compact_history[5])
            $fatal(1,"Grouping changed arithmetic at cycle %0d",cycles);
    end
    initial begin
        for (k=0;k<20;k=k+1) begin expected_pipe[k]=0; exact_pipe[k]=0; end
        for (k=0;k<6;k=k+1) compact_history[k]=0;
        if (!$value$plusargs("VECTORS=%s",vector_path)) $fatal(1,"Missing vectors");
        file=$fopen(vector_path,"r");
        if (!file) $fatal(1,"Cannot open vectors");
        repeat(3) @(negedge clk);
        status=6;
        while(status==6) begin
            status=$fscanf(file,"%d %d %d %d %d %f\n",dx,dy,ev,vv,rr,exact);
            if(status==6) begin
                i_in=dx; q_in=dy; expected=ev; valid=vv; resetn=rr;
                @(negedge clk);
            end
        end
        if (status != -1) $fatal(1,"Malformed vector file");
        valid=0; resetn=1;
        repeat(22) @(negedge clk);
        if(checked<100000) $fatal(1,"Too few checked samples");
        $display("Phase extraction checks passed: %0d samples; selected latency=15 clocks / 60 ns (experimental compact pipeline: 14 clocks); max error=%0d LSB; RMS rounded-reference error=%f LSB",checked,max_error,$sqrt(sum_squared_error/checked));
        $display("Continuous atan2 error: peak=%f LSB RMS=%f LSB",max_exact_error,$sqrt(sum_exact_squared/checked));
        $display("Before output rounding: peak=%f urad RMS=%f urad (%0d samples)",max_raw_error*383.4951969714103,$sqrt(sum_raw_squared/raw_checked)*383.4951969714103,raw_checked);
        $finish;
    end
endmodule
