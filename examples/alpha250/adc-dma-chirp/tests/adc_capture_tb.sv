`timescale 1ns/1ps
module adc_capture_tb;
    reg passed=0;
    reg clk=0;
    always #2 clk=~clk;
    reg aresetn=0, sine_valid=0;
    reg [15:0] sine=16384, adc=0;
    reg [31:0] sample_count=64;
    wire [15:0] dac;
    wire [31:0] status;
    wire [63:0] m_axis_tdata;
    wire m_axis_tvalid,m_axis_tlast;
    reg m_axis_tready=1;
    wire [7:0] m_axis_tkeep;
    adc_capture #(.N_SAMPLES(128),.PACKET_BEATS(8),.TAPER_BITS(3)) dut(.*);
    reg [15:0] expected[0:127];
    integer captured=0,received=0,cycle=0,scenario=0,k,dac_n=0,gain,expected_dac;
    reg collecting=0;
    reg stalled=0;
    reg [63:0] held_data;
    reg held_last;
    always @(posedge clk) begin
        if (aresetn) begin
            if(stalled && (!m_axis_tvalid || m_axis_tdata!==held_data || m_axis_tlast!==held_last))
                $fatal(1,"AXIS changed while stalled");
            stalled=m_axis_tvalid && !m_axis_tready;
            held_data=m_axis_tdata; held_last=m_axis_tlast;
            if(dut.dac_valid) begin
                gain=(dac_n<8) ? dac_n : (63-dac_n<8) ? 63-dac_n : 8;
                expected_dac=($signed(sine)*gain) >>> 3;
                if($signed(dac)!==expected_dac) $fatal(1,"taper alignment at %0d",dac_n);
                dac_n=dac_n+1;
            end
            if (dut.start) collecting=1;
            if (collecting && captured<128) begin
                expected[captured]=adc;
                captured=captured+1;
            end
            if (m_axis_tvalid && m_axis_tready) begin
                for(k=0;k<4;k=k+1)
                    if(m_axis_tdata[16*k +:16] !== expected[4*received+k])
                        $fatal(1,"sample order at beat %0d",received);
                if(m_axis_tlast !== (received%8==7)) $fatal(1,"TLAST");
                if(m_axis_tkeep !== 8'hff) $fatal(1,"TKEEP");
                received=received+1;
            end
        end
    end
    initial begin
        for(scenario=0;scenario<3;scenario=scenario+1) begin
            @(negedge clk); aresetn=0; sine_valid=0;
            repeat(5) @(negedge clk);
            captured=0;received=0;collecting=0;dac_n=0;stalled=0; aresetn=1;
            sine=(scenario==2) ? -16384 : 16384;
            for(cycle=0;cycle<180;cycle=cycle+1) begin
                sine_valid=(cycle<64);
                adc=(scenario==2 && cycle==30) ? 16'h7ffc : cycle;
                m_axis_tready=(scenario==1) ? 0 : (cycle%3!=0);
                @(negedge clk);
            end
            if(scenario==1) begin
                if(!status[2]) $fatal(1,"missing overflow");
            end else begin
                if(received!=32 || !status[1] || status[2]) $fatal(1,"capture incomplete");
                if(status[3] !== (scenario==2)) $fatal(1,"clipping detection");
            end
            if(dac!=0) $fatal(1,"DAC must return to zero");
            if(dac_n!=64) $fatal(1,"DAC sample count");
        end
        $display("PASS adc_capture: order, packet lengths, stalls, overflow, clipping and replay");
        passed=1;
        $finish;
    end
    initial begin #100000; $fatal(1,"timeout"); end
endmodule
