`timescale 1ns/1ps
// Independent AXI master. One burst outstanding; abort drains it before stopping.
// The address fence is in hardware as well as the software reservation check.
module ddr_tester(
    (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF M_AXI, ASSOCIATED_RESET aresetn" *)
    input wire aclk, input wire aresetn,
    input wire [31:0] command, config_word, address, byte_count, seed, burst_beats, pauses, abort_request,
    output reg [31:0] done, busy, errors, mismatches, first_address,
    output reg [63:0] first_expected, first_actual, transferred, cycles,
    output reg [31:0] bursts,
    output wire [31:0] m_axi_awaddr, output wire [7:0] m_axi_awlen,
    output wire [2:0] m_axi_awsize, output wire [1:0] m_axi_awburst,
    output wire m_axi_awlock, output wire [3:0] m_axi_awcache, m_axi_awqos,
    output wire [2:0] m_axi_awprot, output wire m_axi_awvalid, input wire m_axi_awready,
    output wire [63:0] m_axi_wdata, output wire [7:0] m_axi_wstrb,
    output wire m_axi_wlast, m_axi_wvalid, input wire m_axi_wready,
    input wire [1:0] m_axi_bresp, input wire m_axi_bvalid, output wire m_axi_bready,
    output wire [31:0] m_axi_araddr, output wire [7:0] m_axi_arlen,
    output wire [2:0] m_axi_arsize, output wire [1:0] m_axi_arburst,
    output wire m_axi_arlock, output wire [3:0] m_axi_arcache, m_axi_arqos,
    output wire [2:0] m_axi_arprot, output wire m_axi_arvalid, input wire m_axi_arready,
    input wire [63:0] m_axi_rdata, input wire [1:0] m_axi_rresp,
    input wire m_axi_rlast, m_axi_rvalid, output wire m_axi_rready
);
    localparam IDLE=0, PREPARE=1, AW=2, WRITE=3, RESPONSE=4, AR=5, READ=6, FINISH=7, ADVANCE=8;
    reg [3:0] state;
    reg [31:0] previous_command, token, cfg, key, pause_cfg, next_address, remaining;
    reg [31:0] beat_address, burst_address;
    reg [8:0] limit, count, position;
    reg stop;
    wire write_mode = cfg[1:0] == 1;
    // A 4 KiB-aligned address has 512 beats to the boundary (needs ten bits).
    wire [9:0] boundary_beats = 10'd512 - {1'b0,next_address[11:3]};
    wire [31:0] remaining_beats = remaining >> 3;
    wire [9:0] limited = boundary_beats < {1'b0,limit} ? boundary_beats : {1'b0,limit};
    wire [8:0] planned = remaining_beats < limited ? remaining_beats[8:0] : limited[8:0];
    function [31:0] scramble(input [31:0] x);
        reg [31:0] t;
        begin t=x^(x<<13);t=t^(t>>17);scramble=t^(t<<5);end
    endfunction
    function [63:0] pattern(input [31:0] addr, input [31:0] s, input [1:0] kind, input invert);
        reg [63:0] p;
        begin
            case(kind)
                0:p={~addr^s,addr^{s[15:0],s[31:16]}};
                1:p={s,s};
                2:p=64'b1<<addr[8:3];
                3:p={scramble(addr^s^32'h9e3779b9),scramble(addr^s)};
            endcase
            pattern=invert?~p:p;
        end
    endfunction
    wire [63:0] expected=pattern(beat_address,key,cfg[5:4],cfg[2]);
    wire [63:0] mask={{8{cfg[15]}},{8{cfg[14]}},{8{cfg[13]}},{8{cfg[12]}},
                      {8{cfg[11]}},{8{cfg[10]}},{8{cfg[9]}},{8{cfg[8]}}};
    // WVALID may be delayed before a beat, but never withdrawn while stalled.
    reg write_valid;
    assign m_axi_awaddr=burst_address;
    assign m_axi_awlen=count[7:0]-1'b1;
    assign m_axi_awsize=3;assign m_axi_awburst=1;assign m_axi_awlock=0;
    assign m_axi_awcache=3;assign m_axi_awprot=0;assign m_axi_awqos=0;
    assign m_axi_awvalid=state==AW;
    assign m_axi_wdata=expected;assign m_axi_wstrb=cfg[15:8];
    assign m_axi_wlast=position==count-1;assign m_axi_wvalid=state==WRITE&&write_valid;
    assign m_axi_bready=state==RESPONSE;
    assign m_axi_araddr=burst_address;assign m_axi_arlen=count[7:0]-1'b1;
    assign m_axi_arsize=3;assign m_axi_arburst=1;assign m_axi_arlock=0;
    assign m_axi_arcache=3;assign m_axi_arprot=0;assign m_axi_arqos=0;
    assign m_axi_arvalid=state==AR;
    assign m_axi_rready=state==READ&&(stop||cycles[7:0]>=pause_cfg[15:8]);
    always @(posedge aclk) begin
        if(!aresetn) begin
            state<=IDLE;previous_command<=command;done<=0;busy<=0;
            errors<=0;mismatches<=0;first_address<=0;first_expected<=0;first_actual<=0;
            transferred<=0;cycles<=0;bursts<=0;stop<=0;write_valid<=0;
            token<=0;cfg<=0;key<=0;pause_cfg<=0;next_address<=0;remaining<=0;
            beat_address<=0;burst_address<=0;limit<=1;count<=1;position<=0;
        end else begin
            previous_command<=command;
            if(busy) begin cycles<=cycles+1;if(abort_request!=0)stop<=1;end
            case(state)
                IDLE:if(command!=previous_command)begin
                    token<=command;cfg<=config_word;key<=seed;pause_cfg<=pauses;
                    next_address<=address;remaining<=byte_count;stop<=0;
                    errors<=0;mismatches<=0;first_address<=0;first_actual<=0;first_expected<=0;
                    transferred<=0;cycles<=0;bursts<=0;busy<=1;
                    limit<=burst_beats[8:0];
                    if(address<32'h18000000||address>=32'h20000000||address[2:0]!=0||
                       byte_count==0||byte_count[2:0]!=0||byte_count>32'h20000000-address||
                       burst_beats==0||burst_beats>256||
                       (config_word[1:0]!=1&&config_word[1:0]!=2))begin
                        errors<=1;state<=FINISH;
                    end else state<=PREPARE;
                end
                PREPARE:if(remaining==0||stop)state<=FINISH;
                    else begin
                        count<=planned;state<=ADVANCE;
                    end
                ADVANCE:begin
                    position<=0;beat_address<=next_address;burst_address<=next_address;
                    next_address<=next_address+{20'b0,count,3'b0};
                    remaining<=remaining-{20'b0,count,3'b0};
                    bursts<=bursts+1;state<=write_mode?AW:AR;
                end
                AW:if(m_axi_awready)begin state<=WRITE;write_valid<=0;end
                WRITE:begin
                    if(!write_valid&&(stop||cycles[7:0]>=pause_cfg[7:0]))write_valid<=1;
                    if(m_axi_wvalid&&m_axi_wready)begin
                        transferred<=transferred+8;beat_address<=beat_address+8;position<=position+1;
                        write_valid<=0;
                        if(m_axi_wlast)state<=RESPONSE;
                    end
                end
                RESPONSE:if(m_axi_bvalid)begin
                    if(m_axi_bresp!=0)begin errors<=errors+1;stop<=1;end
                    state<=PREPARE;
                end
                AR:if(m_axi_arready)state<=READ;
                READ:if(m_axi_rvalid&&m_axi_rready)begin
                    transferred<=transferred+8;beat_address<=beat_address+8;position<=position+1;
                    if(((m_axi_rdata^expected)&mask)!=0)begin
                        mismatches<=mismatches+1;
                        if(mismatches==0)begin
                            first_address<=beat_address;first_expected<=expected;first_actual<=m_axi_rdata;
                        end
                    end
                    if(m_axi_rresp!=0||m_axi_rlast!=(position==count-1))begin errors<=errors+1;stop<=1;end
                    if(m_axi_rlast)state<=PREPARE;
                end
                FINISH:begin done<=token;busy<=0;state<=IDLE;end
                default:state<=IDLE;
            endcase
        end
    end
endmodule
