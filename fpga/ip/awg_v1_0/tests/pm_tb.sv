`timescale 1ns / 1ps
module pm_tb;
    reg passed = 0;
    reg aclk=0, clk=0, resetn=0;
    always #3.5 aclk=~aclk;
    always #2 clk=~clk;
    reg [11:0] awaddr=0, araddr=0;
    reg awvalid=0, wvalid=0, arvalid=0, bready=0, rready=0;
    reg [31:0] wdata=0;
    reg [3:0] wstrb=0;
    wire awready, wready, arready, bvalid, rvalid;
    wire [1:0] bresp, rresp;
    wire [31:0] rdata;
    wire sample_resetn;
    wire [15:0] mod_phase;
    wire [177:0] mod_tag, sine_tag;
    wire mod_valid, sine_valid;
    wire [23:0] sine;
    wire [103:0] phase_data;
    wire phase_valid, phase_enable;
    wire [15:0] carrier, dac;
    wire carrier_valid, carrier_enable;
    wire [47:0] vendor_phase;
    wire vendor_phase_valid;
    awg_control dut (
        .s_axi_aclk(aclk), .s_axi_aresetn(resetn),
        .s_axi_awaddr(awaddr), .s_axi_awprot(3'b0), .s_axi_awvalid(awvalid), .s_axi_awready(awready),
        .s_axi_wdata(wdata), .s_axi_wstrb(wstrb), .s_axi_wvalid(wvalid), .s_axi_wready(wready),
        .s_axi_bresp(bresp), .s_axi_bvalid(bvalid), .s_axi_bready(bready),
        .s_axi_araddr(araddr), .s_axi_arprot(3'b0), .s_axi_arvalid(arvalid), .s_axi_arready(arready),
        .s_axi_rdata(rdata), .s_axi_rresp(rresp), .s_axi_rvalid(rvalid), .s_axi_rready(rready),
        .sample_clk(clk), .sample_resetn(sample_resetn),
        .m_axis_mod_phase_tdata(mod_phase), .m_axis_mod_phase_tuser(mod_tag), .m_axis_mod_phase_tvalid(mod_valid),
        .s_axis_mod_data_tdata(sine), .s_axis_mod_data_tuser(sine_tag), .s_axis_mod_data_tvalid(sine_valid),
        .m_axis_phase_tdata(phase_data), .m_axis_phase_tuser(phase_enable), .m_axis_phase_tvalid(phase_valid),
        .s_axis_carrier_tdata(carrier), .s_axis_carrier_tuser(carrier_enable), .s_axis_carrier_tvalid(carrier_valid),
        .dac_data(dac)
    );
    pm_modulation modulation_lut (
        .aclk(clk), .aresetn(sample_resetn),
        .s_axis_phase_tdata(mod_phase), .s_axis_phase_tuser(mod_tag), .s_axis_phase_tvalid(mod_valid),
        .m_axis_data_tdata(sine), .m_axis_data_tuser(sine_tag), .m_axis_data_tvalid(sine_valid)
    );
    pm_carrier carrier_dds (
        .aclk(clk), .aresetn(sample_resetn),
        .s_axis_phase_tdata(phase_data), .s_axis_phase_tuser(phase_enable), .s_axis_phase_tvalid(phase_valid),
        .m_axis_data_tdata(carrier), .m_axis_data_tuser(carrier_enable), .m_axis_data_tvalid(carrier_valid),
        .m_axis_phase_tdata(vendor_phase), .m_axis_phase_tvalid(vendor_phase_valid)
    );

    task automatic write(input [11:0] address, input [31:0] data,
                         input [3:0] strobes=15, input integer skew=0,
                         input [1:0] response=0);
        reg [1:0] saved;
        begin
            fork
                begin
                    if (skew<0) repeat(-skew) @(negedge aclk);
                    @(negedge aclk); awaddr=address; awvalid=1;
                    do @(posedge aclk); while (!awready);
                    @(negedge aclk); awvalid=0;
                end
                begin
                    if (skew>0) repeat(skew) @(negedge aclk);
                    @(negedge aclk); wdata=data; wstrb=strobes; wvalid=1;
                    do @(posedge aclk); while (!wready);
                    @(negedge aclk); wvalid=0;
                end
            join
            wait(bvalid); saved=bresp;
            repeat(3) begin
                @(negedge aclk);
                if (!bvalid || bresp!==saved) $fatal(1,"Unstable AXI B response");
            end
            if (saved!==response) $fatal(1,"Write %h: response %d expected %d",address,saved,response);
            bready=1; @(negedge aclk); bready=0;
        end
    endtask

    task automatic read(input [11:0] address, output [31:0] data, input [1:0] response=0);
        reg [31:0] saved;
        begin
            @(negedge aclk); araddr=address; arvalid=1;
            do @(posedge aclk); while(!arready);
            @(negedge aclk); arvalid=0;
            wait(rvalid); saved=rdata;
            repeat(3) begin
                @(negedge aclk);
                if (!rvalid || rdata!==saved) $fatal(1,"Unstable AXI R response");
            end
            if (rresp!==response) $fatal(1,"Read response mismatch");
            data=saved; rready=1; @(negedge aclk); rready=0;
        end
    endtask

    task automatic word(input [11:0] address, input [63:0] data);
        write(address,data[31:0],15,2);
        write(address+4,data[63:32],15,-2);
    endtask

    localparam [48:0] TURN=49'h1000000000000;
    integer selected=1, samples=0, restarts=0;
    reg check_pm=0, arm=0;
    reg [48:0] depth=0, duty=TURN>>1;
    reg [47:0] base=0;
    reg [31:0] rng, gaussian_rng [0:11];
    reg [30:0] prbs;
    function automatic [31:0] next_random(input [31:0] x);
        reg [31:0] y;
        begin y=x^(x<<13); y=y^(y>>17); next_random=y^(y<<5); end
    endfunction

    function automatic [47:0] expected_offset(input integer n);
        reg [47:0] phase;
        reg signed [24:0] value;
        reg signed [75:0] product;
        reg signed [75:0] result;
        integer sum,j;
        real sine_value;
        begin
            phase=(n%8)*(TURN>>3);
            case(selected)
                0: begin
                    sine_value=8388606.0*$sin(6.283185307179586*(n%8)/8.0);
                    value=$rtoi(sine_value>=0 ? sine_value+0.5 : sine_value-0.5);
                end
                1: value=n%8<4 ? 25'sd8388608 : -25'sd8388608;
                2: value={1'b0,phase}<duty ? 25'sd8388608 : -25'sd8388608;
                3: value=n%8<4 ? -8388608+(n%8)*4194304 : 25165824-(n%8)*4194304;
                4: value=(n%8)*2097152-8388608;
                5: value=8388608-(n%8)*2097152;
                6: value=$signed({1'b0,rng[31:8]})-8388608;
                7: begin
                    sum=-3060;
                    for(j=0;j<12;j=j+1) sum=sum+2*gaussian_rng[j][31:24];
                    value=sum*2048;
                end
                8: value=prbs[30] ? 25'sd8388608 : -25'sd8388608;
                default: value=0;
            endcase
            if(selected==9) result=(n/8)%2 ? depth : 0;
            else begin
                product=$signed({1'b0,depth})*value;
                result=(product+4194304)>>>23;
            end
            expected_offset=base+result;
        end
    endfunction

    integer j;
    reg [47:0] expected;
    reg signed [48:0] difference;
    always @(posedge clk) if (phase_valid) begin
        if(phase_data[96]) begin
            restarts=restarts+1;
            if(arm) begin
                arm=0; check_pm=1; samples=0; rng=32'h12345678; prbs=31'h12345678;
                for(j=0;j<12;j=j+1) gaussian_rng[j]=32'h12345678^(32'h9e3779b9*(j+1));
            end
            if(phase_data[47:0]!==0) $fatal(1,"Restart PINC is not zero");
        end
        if(check_pm) begin
            if(samples>0 && samples%8==0) begin
                rng=next_random(rng);
                prbs={prbs[29:0],prbs[30]^prbs[27]};
                for(j=0;j<12;j=j+1) gaussian_rng[j]=next_random(gaussian_rng[j]);
            end
            expected=expected_offset(samples);
            difference=$signed({1'b0,phase_data[95:48]})-$signed({1'b0,expected});
            // Modulo difference, and two LUT output LSBs for the sine model.
            if(difference> $signed({1'b0,TURN>>1})) difference=difference-$signed({1'b0,TURN});
            if(difference< -$signed({1'b0,TURN>>1})) difference=difference+$signed({1'b0,TURN});
            if(selected==0) begin
                if(difference > $signed(depth>>21)+2 || difference < -$signed(depth>>21)-2)
                    $fatal(1,"Sine PM n=%d delta=%d",samples,difference);
            end else if(phase_data[95:48]!==expected)
                $fatal(1,"PM shape=%d n=%d got=%h expected=%h",selected,samples,phase_data[95:48],expected);
            if(!phase_enable) $fatal(1,"Output enable tag lost");
            samples=samples+1;
        end
    end

    // Check the actual vendor DDS's phase packing, latency, TUSER and mute.
    integer head=0, tail=0, reference [0:20000], error;
    reg enabled [0:20000];
    reg [47:0] reference_phase [0:20000];
    integer phase_head=0;
    reg signed [47:0] phase_error;
    reg [47:0] accumulated=0, output_phase;
    real amplitude;
    always @(posedge clk) begin
        if(!sample_resetn) begin head=0; tail=0; phase_head=0; accumulated=0; end
        else begin
            if(phase_valid) begin
                accumulated=(phase_data[96] ? 0 : accumulated)+phase_data[47:0];
                output_phase=accumulated+phase_data[95:48];
                amplitude=32766.0*$sin(6.283185307179586*$itor(output_phase[47:24])/16777216.0);
                reference[tail]=$rtoi(amplitude>=0 ? amplitude+0.5 : amplitude-0.5);
                enabled[tail]=phase_enable;
                reference_phase[tail]=output_phase;
                tail=tail+1;
            end
            if(carrier_valid) begin
                if(head>=tail) $fatal(1,"DDS produced extra sample");
                if(carrier_enable!==enabled[head]) $fatal(1,"DDS TUSER ordering mismatch");
                error=$signed(carrier)-reference[head];
                // Phase dithering plus truncation spans up to two 14-bit LUT
                // angle bins, about 25.2 amplitude codes at maximum slope.
                if(error>27 || error < -27) $fatal(1,"Carrier sample mismatch %d",error);
                if(!carrier_enable && dac!==0) $fatal(1,"Muted DAC is not zero");
                head=head+1;
            end
            if(vendor_phase_valid) begin
                phase_error=vendor_phase-reference_phase[phase_head];
                // PHASE output includes the vendor's phase dither. Check
                // native packing to within one 14-bit LUT address bin.
                if(phase_head>=tail || phase_error > 48'sh000400000000 || phase_error < -48'sh000400000000)
                    $fatal(1,"Vendor phase mismatch at %d: got %h expected %h",phase_head,vendor_phase,reference_phase[phase_head]);
                phase_head=phase_head+1;
            end
        end
    end

    task automatic run_shape(input integer shape, input [48:0] deviation, input [48:0] pulse_duty);
        begin
            check_pm=0; selected=shape; depth=deviation; duty=pulse_duty;
            base=48'h123456789abc;
            word('h20,48'h100000000);
            word('h28,base);
            word('h30,TURN>>3);
            word('h38,0);
            word('h40,depth);
            word('h48,duty);
            write('h50,32'h12345678);
            write('h54,(shape<<8)|3);
            arm=1; write('h10,7);
            wait(!arm); wait(samples>=96);
            check_pm=0;
        end
    endtask

    reg [31:0] data;
    initial begin
        repeat(5) @(negedge aclk); resetn=1;
        read(0,data); if(data!==32'h504d0001) $fatal(1,"Wrong ID");
        read(4,data); if(data!==1023) $fatal(1,"Wrong capabilities");
        read(8,data); if(data!==32'h1f0e1830) $fatal(1,"Wrong precision metadata");
        write('h20,32'h12345678,15,4);
        write('h20,32'hffffffff,2,-4);
        read('h20,data); if(data!==32'h1234ff78) $fatal(1,"WSTRB failure");
        write('h24,32'hffffffff);
        read('h24,data); if(data!==32'hffff) $fatal(1,"Upper bits not masked");
        write('h00,0,15,0,2);
        write('h21,0,15,0,2);
        read('h14,data,2);
        word('h40,TURN+1); write('h10,1,15,0,2);
        word('h40,0); write('h54,'hf03); write('h10,1,15,0,2);
        write('h54,0); write('h10,32'hffffff01,1); // Unstrobed command bytes ignored.
        for(integer mode=0;mode<10;mode=mode+1) run_shape(mode,TURN>>3,3*(TURN>>3));
        run_shape(4,TURN,TURN>>1); // Full turn must survive scaling.
        run_shape(1,1,TURN>>1); // Native one-LSB deviation, exact endpoints.
        run_shape(2,17,0); run_shape(2,17,TURN); // 0 and 100 percent pulse duty.
        check_pm=0;
        write('h54,0); write('h10,1);
        repeat(100) @(negedge clk);
        if(dac!==0) $fatal(1,"Mute did not reach DAC");
        if(head<1000) $fatal(1,"Too few DDS samples checked");
        // Reset while active and reconfigure, ensuring request toggles recover.
        @(negedge aclk); resetn=0;
        repeat(5) @(negedge aclk); resetn=1;
        run_shape(3,17,TURN>>1);
        passed=1;
        $display("PASS: all internal PM shapes, native precision, AXI, CDC, real DDS and reset");
        $finish;
    end
    initial begin #2000000; $fatal(1,"Simulation timeout"); end
endmodule
