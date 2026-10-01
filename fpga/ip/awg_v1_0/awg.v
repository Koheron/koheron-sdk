`timescale 1ns / 1ps

// One independently configurable carrier. Connect M_AXIS_PHASE to a Xilinx
// streaming DDS; the integration helper supplies the optional modulation LUT.
module awg #(
    parameter integer PHASE_WIDTH = 48,
    parameter integer OUTPUT_WIDTH = 16,
    parameter integer MOD_WIDTH = 24,
    parameter integer LUT_BITS = 14,
    parameter integer PRBS_WIDTH = 31,
    parameter integer ENABLE_SINE = 1,
    parameter integer ENABLE_SQUARE = 1,
    parameter integer ENABLE_PULSE = 1,
    parameter integer ENABLE_TRIANGLE = 1,
    parameter integer ENABLE_UP_RAMP = 1,
    parameter integer ENABLE_DOWN_RAMP = 1,
    parameter integer ENABLE_UNIFORM = 1,
    parameter integer ENABLE_GAUSSIAN = 1,
    parameter integer ENABLE_PRBS = 1,
    parameter integer ENABLE_BPSK = 1,
    parameter integer AXI_ADDR_WIDTH = 12
) (
    input wire s_axi_aclk, s_axi_aresetn,
    input wire [AXI_ADDR_WIDTH-1:0] s_axi_awaddr,
    input wire [2:0] s_axi_awprot,
    input wire s_axi_awvalid,
    output wire s_axi_awready,
    input wire [31:0] s_axi_wdata,
    input wire [3:0] s_axi_wstrb,
    input wire s_axi_wvalid,
    output wire s_axi_wready,
    output reg [1:0] s_axi_bresp,
    output reg s_axi_bvalid,
    input wire s_axi_bready,
    input wire [AXI_ADDR_WIDTH-1:0] s_axi_araddr,
    input wire [2:0] s_axi_arprot,
    input wire s_axi_arvalid,
    output wire s_axi_arready,
    output reg [31:0] s_axi_rdata,
    output reg [1:0] s_axi_rresp,
    output reg s_axi_rvalid,
    input wire s_axi_rready,

    input wire sample_clk,
    output wire sample_resetn,
    output wire [((LUT_BITS+7)/8)*8-1:0] m_axis_mod_phase_tdata,
    output wire [3*PHASE_WIDTH+MOD_WIDTH+9:0] m_axis_mod_phase_tuser,
    output wire m_axis_mod_phase_tvalid,
    input wire [((MOD_WIDTH+7)/8)*8-1:0] s_axis_mod_data_tdata,
    input wire [3*PHASE_WIDTH+MOD_WIDTH+9:0] s_axis_mod_data_tuser,
    input wire s_axis_mod_data_tvalid,
    output wire [2*((PHASE_WIDTH+7)/8)*8+7:0] m_axis_phase_tdata,
    output wire [0:0] m_axis_phase_tuser,
    output wire m_axis_phase_tvalid,
    input wire [((OUTPUT_WIDTH+7)/8)*8-1:0] s_axis_carrier_tdata,
    input wire [0:0] s_axis_carrier_tuser,
    input wire s_axis_carrier_tvalid,
    output wire [OUTPUT_WIDTH-1:0] dac_data
);
    localparam integer PHASE_BYTES = (PHASE_WIDTH+7)/8;
    localparam integer MOD_BYTES = (MOD_WIDTH+7)/8;
    localparam integer LUT_BYTES = (LUT_BITS+7)/8;
    localparam integer TAG_WIDTH = 3*PHASE_WIDTH+MOD_WIDTH+10;
    assign dac_data = s_axis_carrier_tvalid && s_axis_carrier_tuser[0] ?
        s_axis_carrier_tdata[OUTPUT_WIDTH-1:0] : {OUTPUT_WIDTH{1'b0}};
    localparam integer SOURCE_MASK = ENABLE_SINE + 2*ENABLE_SQUARE +
        4*ENABLE_PULSE + 8*ENABLE_TRIANGLE + 16*ENABLE_UP_RAMP +
        32*ENABLE_DOWN_RAMP + 64*ENABLE_UNIFORM + 128*ENABLE_GAUSSIAN +
        256*ENABLE_PRBS + 512*ENABLE_BPSK;
    localparam [63:0] PHASE_MASK = (64'h1 << PHASE_WIDTH)-1;
    localparam [63:0] FULL_TURN = 64'h1 << PHASE_WIDTH;
    localparam integer CFG_WIDTH = 14*32;

    // AXI shadow registers and a stable-data request/acknowledge mailbox.
    reg [31:0] shadow [0:13];
    reg [CFG_WIDTH+1:0] mailbox;
    reg request, acknowledge;
    (* ASYNC_REG = "TRUE" *) reg [1:0] ack_sync;
    (* ASYNC_REG = "TRUE" *) reg [1:0] request_sync;
    (* ASYNC_REG = "TRUE" *) reg [1:0] reset_sync;
    // The two CDC stages stay together. Their third, distribution stage may
    // be replicated so thousands of sample-domain reset sinks meet timing.
    (* max_fanout = 16 *) reg sample_resetn_reg;
    wire busy = request != ack_sync[1];
    assign sample_resetn = sample_resetn_reg;
    always @(posedge sample_clk or negedge s_axi_aresetn) begin
        if (!s_axi_aresetn) begin reset_sync <= 0; sample_resetn_reg <= 0; end
        else begin
            reset_sync <= {reset_sync[0], 1'b1};
            sample_resetn_reg <= reset_sync[1];
        end
    end
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) ack_sync <= 0;
        else ack_sync <= {ack_sync[0], acknowledge};
    end
    always @(posedge sample_clk) begin
        if (!sample_resetn) request_sync <= 0;
        else request_sync <= {request_sync[0], request};
    end

    reg [CFG_WIDTH-1:0] active;
    // Preserve the mailbox receiver boundary against SRL absorption/retiming.
    (* DONT_TOUCH = "TRUE" *) reg restart_carrier;
    reg restart_mod;
    // A registered, replicated enable avoids a high-fanout comparator on the
    // sample-clock path. The extra settling cycle also strengthens the CDC.
    (* max_fanout = 16 *) reg capture_pending;
    always @(posedge sample_clk) begin
        if (!sample_resetn) begin
            active <= 0;
            acknowledge <= 0;
            restart_carrier <= 0;
            restart_mod <= 0;
            capture_pending <= 0;
        end else begin
            restart_carrier <= 0;
            restart_mod <= 0;
            if (capture_pending) begin
                active <= mailbox[CFG_WIDTH-1:0];
                restart_carrier <= mailbox[CFG_WIDTH];
                restart_mod <= mailbox[CFG_WIDTH+1];
                acknowledge <= request_sync[1];
                capture_pending <= 0;
            end else if (request_sync[1] != acknowledge) capture_pending <= 1;
        end
    end

    reg aw_pending, w_pending;
    reg [AXI_ADDR_WIDTH-1:0] awaddr;
    reg [31:0] wdata;
    reg [3:0] wstrb;
    wire [31:0] command_value = wdata & {{8{wstrb[3]}}, {8{wstrb[2]}}, {8{wstrb[1]}}, {8{wstrb[0]}}};
    assign s_axi_awready = !aw_pending && !s_axi_bvalid;
    assign s_axi_wready = !w_pending && !s_axi_bvalid;
    assign s_axi_arready = !s_axi_rvalid;

    wire [63:0] depth_shadow = {shadow[9], shadow[8]};
    wire [63:0] duty_shadow = {shadow[11], shadow[10]};
    wire [3:0] shape_shadow = shadow[13][11:8];
    wire invalid_config = depth_shadow > FULL_TURN || duty_shadow > FULL_TURN ||
        shape_shadow > 9 ||
        (shadow[13][1] && !(SOURCE_MASK & (1 << shape_shadow))) ||
        ((shape_shadow == 6 || shape_shadow == 7) && shadow[12] == 0) ||
        (shape_shadow == 8 && (shadow[12] & ((64'h1 << PRBS_WIDTH)-1)) == 0);

    function [31:0] register_mask;
        input integer index;
        begin
            case (index)
                1,3,5,7: register_mask = PHASE_MASK >> 32;
                9,11: register_mask = ((FULL_TURN << 1)-1) >> 32;
                13: register_mask = 32'h00000f03;
                default: register_mask = 32'hffffffff;
            endcase
        end
    endfunction

    integer i, b, idx;
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            aw_pending <= 0; w_pending <= 0;
            awaddr <= 0; wdata <= 0; wstrb <= 0;
            s_axi_bvalid <= 0; s_axi_bresp <= 0;
            request <= 0; mailbox <= 0;
            for (i=0; i<14; i=i+1) shadow[i] <= 0;
            shadow[10] <= (FULL_TURN >> 1);
            shadow[11] <= (FULL_TURN >> 33);
            shadow[12] <= 1;
        end else begin
            if (s_axi_awvalid && s_axi_awready) begin
                aw_pending <= 1; awaddr <= s_axi_awaddr;
            end
            if (s_axi_wvalid && s_axi_wready) begin
                w_pending <= 1; wdata <= s_axi_wdata; wstrb <= s_axi_wstrb;
            end
            if (s_axi_bvalid && s_axi_bready) s_axi_bvalid <= 0;
            if (aw_pending && w_pending && !s_axi_bvalid) begin
                aw_pending <= 0; w_pending <= 0;
                s_axi_bvalid <= 1; s_axi_bresp <= 0;
                if (awaddr[1:0] != 0) s_axi_bresp <= 2;
                else if (awaddr == 'h10) begin
                    if (command_value[0]) begin
                        if (busy || invalid_config || (command_value & ~32'h7) != 0) s_axi_bresp <= 2;
                        else begin
                            for (i=0; i<14; i=i+1) mailbox[i*32 +: 32] <= shadow[i];
                            mailbox[CFG_WIDTH +: 2] <= command_value[2:1];
                            request <= ~request;
                        end
                    end else if (command_value != 0) s_axi_bresp <= 2;
                end else if (awaddr >= 'h20 && awaddr <= 'h54) begin
                    idx = (awaddr-'h20) >> 2;
                    for (b=0; b<4; b=b+1)
                        if (wstrb[b]) shadow[idx][b*8 +: 8] <= wdata[b*8 +: 8] & (register_mask(idx) >> (b*8));
                end else s_axi_bresp <= 2;
            end
        end
    end

    integer ridx;
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            s_axi_rvalid <= 0; s_axi_rdata <= 0; s_axi_rresp <= 0;
        end else begin
            if (s_axi_rvalid && s_axi_rready) s_axi_rvalid <= 0;
            if (s_axi_arvalid && s_axi_arready) begin
                s_axi_rvalid <= 1; s_axi_rresp <= 0; s_axi_rdata <= 0;
                if (s_axi_araddr[1:0] != 0) s_axi_rresp <= 2;
                else case (s_axi_araddr)
                    'h00: s_axi_rdata <= 32'h504d0001;
                    'h04: s_axi_rdata <= SOURCE_MASK;
                    'h08: s_axi_rdata <= PHASE_WIDTH | (MOD_WIDTH << 8) | (LUT_BITS << 16) | (PRBS_WIDTH << 24);
                    'h0c: s_axi_rdata <= {31'b0, busy};
                    'h10: s_axi_rdata <= 0;
                    'h18: s_axi_rdata <= OUTPUT_WIDTH;
                    default: begin
                        if (s_axi_araddr >= 'h20 && s_axi_araddr <= 'h54) begin
                            ridx = (s_axi_araddr-'h20) >> 2;
                            s_axi_rdata <= shadow[ridx];
                        end else s_axi_rresp <= 2;
                    end
                endcase
            end
        end
    end

    wire [PHASE_WIDTH-1:0] mod_increment = active[128 +: PHASE_WIDTH];
    wire [PHASE_WIDTH-1:0] mod_offset = active[192 +: PHASE_WIDTH];
    wire [PHASE_WIDTH:0] duty = active[320 +: PHASE_WIDTH+1];
    wire [3:0] shape = active[424 +: 4];
    wire signed [MOD_WIDTH:0] raw;
    wire bit_state;
    wire [LUT_BITS-1:0] lut_phase;
    generate if (SOURCE_MASK != 0) begin : g_sources
    pm_waveform #(.PHASE_WIDTH(PHASE_WIDTH), .MOD_WIDTH(MOD_WIDTH),
        .LUT_BITS(LUT_BITS), .SOURCE_MASK(SOURCE_MASK), .PRBS_WIDTH(PRBS_WIDTH)) sources (
        .clk(sample_clk), .resetn(sample_resetn), .restart(restart_mod),
        .increment(mod_increment), .phase_offset(mod_offset), .duty(duty),
        .seed(active[384 +: 32]), .shape(shape), .lut_phase(lut_phase),
        .value(raw), .bpsk_state(bit_state)
    );
    end else begin : g_no_sources
        assign raw = 0;
        assign bit_state = 0;
        assign lut_phase = 0;
    end endgenerate
    wire [TAG_WIDTH-1:0] immediate_tag = {restart_carrier, active[416], active[417] && (SOURCE_MASK != 0),
        shape, bit_state, SOURCE_MASK != 0 ? active[256 +: PHASE_WIDTH+1] : {(PHASE_WIDTH+1){1'b0}},
        active[64 +: PHASE_WIDTH], active[0 +: PHASE_WIDTH], {(MOD_WIDTH+1){1'b0}}};
    reg [TAG_WIDTH-1:0] tag_pipe [0:4];
    reg [LUT_BITS-1:0] phase_pipe [0:4];
    reg [4:0] source_valid_pipe;
    integer stage;
    always @(posedge sample_clk) begin
        if (!sample_resetn) source_valid_pipe <= 0;
        else source_valid_pipe <= {source_valid_pipe[3:0], 1'b1};
        tag_pipe[0] <= immediate_tag;
        phase_pipe[0] <= lut_phase;
        for (stage=1; stage<5; stage=stage+1) begin
            tag_pipe[stage] <= tag_pipe[stage-1];
            phase_pipe[stage] <= phase_pipe[stage-1];
        end
    end
    wire [TAG_WIDTH-1:0] local_tag = {tag_pipe[4][TAG_WIDTH-1:MOD_WIDTH+1], raw};
    assign m_axis_mod_phase_tdata = ENABLE_SINE ? {{(LUT_BYTES*8-LUT_BITS){1'b0}}, phase_pipe[4]} : 0;
    assign m_axis_mod_phase_tuser = ENABLE_SINE ? local_tag : 0;
    assign m_axis_mod_phase_tvalid = ENABLE_SINE && source_valid_pipe[4];

    wire [TAG_WIDTH-1:0] tag;
    wire source_valid;
    wire signed [MOD_WIDTH:0] source_value;
    generate if (ENABLE_SINE) begin : g_sine
        assign tag = s_axis_mod_data_tuser;
        assign source_valid = s_axis_mod_data_tvalid;
        // The LUT carries all settings and non-sine samples through TUSER.
        assign source_value = tag[MOD_WIDTH+1+3*PHASE_WIDTH+2 +: 4] == 0 ?
            $signed(s_axis_mod_data_tdata[MOD_WIDTH-1:0]) : $signed(tag[0 +: MOD_WIDTH+1]);
    end else begin : g_no_sine
        assign tag = local_tag;
        assign source_valid = source_valid_pipe[4];
        assign source_value = raw;
    end endgenerate

    localparam integer INC_LSB = MOD_WIDTH+1;
    localparam integer BASE_LSB = INC_LSB+PHASE_WIDTH;
    localparam integer DEPTH_LSB = BASE_LSB+PHASE_WIDTH;
    localparam integer BIT_LSB = DEPTH_LSB+PHASE_WIDTH+1;
    localparam integer SHAPE_LSB = BIT_LSB+1;
    pm_scale #(.PHASE_WIDTH(PHASE_WIDTH), .MOD_WIDTH(MOD_WIDTH),
        .SOURCE_MASK(SOURCE_MASK), .PHASE_BYTES(PHASE_BYTES)) scaler (
        .clk(sample_clk), .resetn(sample_resetn), .valid(source_valid),
        .modulation(source_value), .increment(tag[INC_LSB +: PHASE_WIDTH]),
        .base_phase(tag[BASE_LSB +: PHASE_WIDTH]), .deviation(tag[DEPTH_LSB +: PHASE_WIDTH+1]),
        .shape(tag[SHAPE_LSB +: 4]), .bpsk_state(tag[BIT_LSB]),
        .pm_enabled(tag[SHAPE_LSB+4]), .output_enabled(tag[SHAPE_LSB+5]),
        .restart(tag[SHAPE_LSB+6]), .tdata(m_axis_phase_tdata),
        .tuser(m_axis_phase_tuser), .tvalid(m_axis_phase_tvalid)
    );
endmodule
