`timescale 1 ns / 1 ps
module phase_extraction_test;
    reg clk=0, resetn=0, valid=0;
    reg signed [23:0] i_in=0, q_in=0;
    wire vf, vr;
    wire signed [23:0] fast, reference_phase;
    phase_extractor #(.RESIDUAL_CORRECTION(1)) dut(clk,resetn,valid,i_in,q_in,vf,fast);
    phase_extractor #(.RESIDUAL_CORRECTION(0)) reference_pipeline(clk,resetn,valid,i_in,q_in,vr,reference_phase);
    always #2 clk=~clk;
    real exact=0, exact_pipe[0:18];
    reg [18:0] valid_pipe=0;
    integer history[0:4];
    integer k,dx,dy,ev,vv,rr,file,status,checked=0,cycles=0,difference;
    real error,peak=0,squared=0,raw,raw_error,raw_peak=0,raw_squared=0;
    reg [2047:0] vector_path;
    always @(posedge clk) begin
        for(k=18;k>0;k=k-1) exact_pipe[k]=exact_pipe[k-1];
        exact_pipe[0]=exact*256.0;
        valid_pipe=resetn ? {valid_pipe[17:0],valid} : 0;
        for(k=4;k>0;k=k-1) history[k]=history[k-1];
        history[0]=fast;
        cycles=cycles+1;
        #0.1;
        if(vf !== valid_pipe[13] || vr !== valid_pipe[18]) $fatal(1,"Latency/reset mismatch at cycle %0d",cycles);
        if(vf) begin
            if ((^fast) === 1'bx) $fatal(1,"Unknown phase output");
            error=$signed(fast)-exact_pipe[13];
            if(error>2097152) error=error-4194304;
            if(error< -2097152) error=error+4194304;
            if(error<0) error=-error;
            error=error*1.49802811316957;
            if(error>1.5) $fatal(1,"Angular error exceeds 1.5 urad: %f at cycle %0d",error,cycles);
            if(error>peak) peak=error;
            squared=squared+error*error;
            raw=dut.residual_completion.completion.zero3 ? 0.0 :
                $signed(dut.residual_completion.completion.accumulated) /
                (1048576.0*(2**dut.residual_completion.completion.scale3))-0.5;
            raw_error=raw-exact_pipe[13];
            if(raw_error>2097152) raw_error=raw_error-4194304;
            if(raw_error< -2097152) raw_error=raw_error+4194304;
            if(raw_error<0) raw_error=-raw_error;
            raw_error=raw_error*1.49802811316957;
            if(raw_error>0.6) $fatal(1,"Residual approximation exceeds 0.6 urad: %f",raw_error);
            if(raw_error>raw_peak) raw_peak=raw_error;
            raw_squared=raw_squared+raw_error*raw_error;
            checked=checked+1;
        end
        if(vr) begin
            difference=$signed(reference_phase)-history[4];
            if(difference>2097152) difference=difference-4194304;
            if(difference< -2097152) difference=difference+4194304;
            if(difference>2 || difference< -2) $fatal(1,"Residual/CORDIC phase disagreement %0d counts",difference);
        end
    end
    initial begin
        for(k=0;k<19;k=k+1) exact_pipe[k]=0;
        for(k=0;k<5;k=k+1) history[k]=0;
        if(!$value$plusargs("VECTORS=%s",vector_path)) $fatal(1,"Missing vectors");
        file=$fopen(vector_path,"r");
        if(!file) $fatal(1,"Cannot open vectors");
        // DSP primitive startup GSR lasts 100 ns in the vendor simulator.
        repeat(30) @(negedge clk);
        status=6;
        while(status==6) begin
            status=$fscanf(file,"%d %d %d %d %d %f\n",dx,dy,ev,vv,rr,exact);
            if(status==6) begin
                i_in=dx; q_in=dy; valid=vv; resetn=rr;
                @(negedge clk);
            end
        end
        if(status != -1) $fatal(1,"Malformed vector file");
        valid=0; resetn=1;
        repeat(21) @(negedge clk);
        if(checked<100000) $fatal(1,"Too few checked samples");
        $display("Phase extraction checks passed: %0d samples; selected latency=14 clocks / 56 ns; peak=%f urad RMS=%f urad",checked,peak,$sqrt(squared/checked));
        $display("Before output rounding: peak=%f urad RMS=%f urad",raw_peak,$sqrt(raw_squared/checked));
        $finish;
    end
endmodule
