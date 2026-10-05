`timescale 1ns/1ps
module phase_quantizer_tb;
    parameter BASE_SHIFT=8;
    reg aclk=0, aresetn=0;
    always #4 aclk=~aclk;
    reg [3:0] requested_bits=0;
    reg upstream_overflow=0;
    reg [39:0] s_axis_tdata=0;
    reg s_axis_tvalid=0, m_axis_tready=0;
    wire s_axis_tready, m_axis_tvalid, m_axis_tlast;
    wire [31:0] m_axis_tdata, packet_status;
    wire [4:0] sample_status;
    phase_quantizer #(.PKT_LENGTH(16), .BASE_SHIFT(BASE_SHIFT)) dut(.*);
    reg [31:0] expected_data[0:4095];
    reg expected_clip[0:4095];
    integer expected_bits[0:4095];
    integer produced=0, consumed=0, cycles=0, active=0, packet_number=0;
    integer coverage=0;
    reg packet_clip=0;
    reg [31:0] expected_status=0;
    reg stalled=0;
    reg [37:0] held;
    longint signed value, magnitude, divisor, quotient, remainder, rounded;

    initial begin
        repeat(4) @(negedge aclk);
        aresetn=1;
    end
    always @(posedge aclk) if (aresetn) begin
        cycles=cycles+1;
        if(cycles>20000) $fatal(1,"stream did not drain");
        if(stalled && (!m_axis_tvalid || {sample_status,m_axis_tlast,m_axis_tdata} !== held))
            $fatal(1,"output changed under backpressure");
        stalled=m_axis_tvalid && !m_axis_tready;
        held={sample_status,m_axis_tlast,m_axis_tdata};
        if(s_axis_tvalid && s_axis_tready) begin
            if(produced%16==0) active=requested_bits<=8 ? requested_bits : 0;
            coverage=coverage | (1<<active);
            value=$signed(s_axis_tdata);
            magnitude=value<0 ? -value : value;
            divisor=64'd1<<(BASE_SHIFT-active);
            quotient=magnitude/divisor;
            remainder=magnitude%divisor;
            if(2*remainder>divisor || (2*remainder==divisor && quotient%2))
                quotient=quotient+1;
            rounded=value<0 ? -quotient : quotient;
            expected_clip[produced]=upstream_overflow || rounded>64'sd2147483647 || rounded<-64'sd2147483648;
            expected_data[produced]=rounded>64'sd2147483647 ? 32'h7fffffff :
                rounded<-64'sd2147483648 ? 32'h80000000 : rounded;
            expected_bits[produced]=active;
            produced=produced+1;
        end
        if(m_axis_tvalid && m_axis_tready) begin
            if(consumed>=produced || m_axis_tdata !== expected_data[consumed] ||
               m_axis_tlast !== (consumed%16==15))
                $fatal(1,"wrong rounded sample or packet boundary at %d",consumed);
            if (sample_status !== {expected_clip[consumed], 4'(expected_bits[consumed])})
                $fatal(1,"sample metadata mismatch at %d",consumed);
            packet_clip=packet_clip | expected_clip[consumed];
            if(m_axis_tlast) begin
                packet_number=packet_number+1;
                expected_status=(packet_number<<8) | (packet_clip<<4) | expected_bits[consumed];
                packet_clip=0;
            end
            consumed=consumed+1;
        end
    end
    always @(negedge aclk) if(aresetn) begin
        if(packet_status !== expected_status) $fatal(1,"packet metadata mismatch");
        if(consumed==4096) begin
            if(coverage!=511) $fatal(1,"not every precision was exercised");
            $display("4096 samples passed: all precisions, signed ties, output/upstream overflow, stalls and packet metadata");
            $finish;
        end
        requested_bits=$urandom_range(0,12); // includes rejected hardware values
        m_axis_tready=$urandom_range(0,3)!=0 && cycles%101<80;
        if(s_axis_tready || !s_axis_tvalid) begin
            s_axis_tvalid=produced<4096 && $urandom_range(0,7)!=0;
            // Alternate clean, upstream-overflow-only and saturation packets.
            // Include the final sample in upstream overflow metadata checks.
            upstream_overflow=(produced/16)%3==0 && produced%16==15;
            if((produced/16)%3<2) s_axis_tdata=$signed($urandom_range(0,4095))-2048;
            else case(produced%16)
                0: s_axis_tdata=0;
                1: s_axis_tdata=1;
                2: s_axis_tdata=-1;
                3: s_axis_tdata=64'd1<<(BASE_SHIFT-1);
                4: s_axis_tdata=-(64'sd1<<(BASE_SHIFT-1));
                5: s_axis_tdata=64'd3<<(BASE_SHIFT-1);
                6: s_axis_tdata=-(64'sd3<<(BASE_SHIFT-1));
                7: s_axis_tdata=40'h7fffffffff;
                8: s_axis_tdata=40'h8000000000;
                9: s_axis_tdata=64'sd2147483647;
                10: s_axis_tdata=-64'sd2147483648;
                11: s_axis_tdata=64'sd2147483648;
                12: s_axis_tdata=-64'sd2147483649;
                default: s_axis_tdata={$urandom,$urandom};
            endcase
        end
    end
endmodule
