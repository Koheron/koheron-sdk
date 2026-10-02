`timescale 1ns / 1ps

// Internal channel: AXI controller plus carrier DDS and optional sine LUT.
module awg_channel #(
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
    parameter integer CHANNEL_COUNT = 1,
    parameter integer AXI_ADDR_WIDTH = 12
) (
    input wire s_axi_aclk, s_axi_aresetn,
    input wire [11:0] s_axi_awaddr,
    input wire [2:0] s_axi_awprot,
    input wire s_axi_awvalid,
    output wire s_axi_awready,
    input wire [31:0] s_axi_wdata,
    input wire [3:0] s_axi_wstrb,
    input wire s_axi_wvalid,
    output wire s_axi_wready,
    output wire [1:0] s_axi_bresp,
    output wire s_axi_bvalid,
    input wire s_axi_bready,
    input wire [11:0] s_axi_araddr,
    input wire [2:0] s_axi_arprot,
    input wire s_axi_arvalid,
    output wire s_axi_arready,
    output wire [31:0] s_axi_rdata,
    output wire [1:0] s_axi_rresp,
    output wire s_axi_rvalid,
    input wire s_axi_rready,

    input wire sample_clk,
    output wire [OUTPUT_WIDTH-1:0] dac_data
);
    localparam integer PBYTES = (PHASE_WIDTH+7)/8;
    localparam integer MBYTES = (MOD_WIDTH+7)/8;
    localparam integer LBYTES = (LUT_BITS+7)/8;
    localparam integer OBYTES = (OUTPUT_WIDTH+7)/8;
    localparam integer TAG_WIDTH = 3*PHASE_WIDTH+MOD_WIDTH+10;
    wire sample_resetn;
    wire [LBYTES*8-1:0] mod_phase;
    wire [TAG_WIDTH-1:0] mod_tag, sine_tag;
    wire mod_valid, sine_valid;
    wire [MBYTES*8-1:0] sine;
    wire [2*PBYTES*8+7:0] phase_data;
    wire phase_valid, phase_enable;
    wire [OBYTES*8-1:0] carrier;
    wire carrier_valid, carrier_enable;
    awg_control #(
        .PHASE_WIDTH(PHASE_WIDTH), .OUTPUT_WIDTH(OUTPUT_WIDTH), .MOD_WIDTH(MOD_WIDTH),
        .LUT_BITS(LUT_BITS), .PRBS_WIDTH(PRBS_WIDTH), .CHANNEL_COUNT(CHANNEL_COUNT),
        .ENABLE_SINE(ENABLE_SINE), .ENABLE_SQUARE(ENABLE_SQUARE), .ENABLE_PULSE(ENABLE_PULSE),
        .ENABLE_TRIANGLE(ENABLE_TRIANGLE), .ENABLE_UP_RAMP(ENABLE_UP_RAMP),
        .ENABLE_DOWN_RAMP(ENABLE_DOWN_RAMP), .ENABLE_UNIFORM(ENABLE_UNIFORM),
        .ENABLE_GAUSSIAN(ENABLE_GAUSSIAN), .ENABLE_PRBS(ENABLE_PRBS), .ENABLE_BPSK(ENABLE_BPSK)
    ) controller (
        .s_axi_aclk(s_axi_aclk),
        .s_axi_aresetn(s_axi_aresetn),
        .s_axi_awaddr(s_axi_awaddr),
        .s_axi_awprot(s_axi_awprot),
        .s_axi_awvalid(s_axi_awvalid),
        .s_axi_awready(s_axi_awready),
        .s_axi_wdata(s_axi_wdata),
        .s_axi_wstrb(s_axi_wstrb),
        .s_axi_wvalid(s_axi_wvalid),
        .s_axi_wready(s_axi_wready),
        .s_axi_bresp(s_axi_bresp),
        .s_axi_bvalid(s_axi_bvalid),
        .s_axi_bready(s_axi_bready),
        .s_axi_araddr(s_axi_araddr),
        .s_axi_arprot(s_axi_arprot),
        .s_axi_arvalid(s_axi_arvalid),
        .s_axi_arready(s_axi_arready),
        .s_axi_rdata(s_axi_rdata),
        .s_axi_rresp(s_axi_rresp),
        .s_axi_rvalid(s_axi_rvalid),
        .s_axi_rready(s_axi_rready),
        .sample_clk(sample_clk), .sample_resetn(sample_resetn),
        .m_axis_mod_phase_tdata(mod_phase), .m_axis_mod_phase_tuser(mod_tag), .m_axis_mod_phase_tvalid(mod_valid),
        .s_axis_mod_data_tdata(sine), .s_axis_mod_data_tuser(sine_tag), .s_axis_mod_data_tvalid(sine_valid),
        .m_axis_phase_tdata(phase_data), .m_axis_phase_tuser(phase_enable), .m_axis_phase_tvalid(phase_valid),
        .s_axis_carrier_tdata(carrier), .s_axis_carrier_tuser(carrier_enable), .s_axis_carrier_tvalid(carrier_valid),
        .dac_data(dac_data)
    );
    dds_pm_carrier carrier_dds (
        .aclk(sample_clk), .aresetn(sample_resetn),
        .s_axis_phase_tdata(phase_data), .s_axis_phase_tuser(phase_enable), .s_axis_phase_tvalid(phase_valid),
        .m_axis_data_tdata(carrier), .m_axis_data_tuser(carrier_enable), .m_axis_data_tvalid(carrier_valid)
    );
    generate if (ENABLE_SINE) begin : g_sine
        dds_pm_modulation modulation_lut (
            .aclk(sample_clk), .aresetn(sample_resetn),
            .s_axis_phase_tdata(mod_phase), .s_axis_phase_tuser(mod_tag), .s_axis_phase_tvalid(mod_valid),
            .m_axis_data_tdata(sine), .m_axis_data_tuser(sine_tag), .m_axis_data_tvalid(sine_valid)
        );
    end else begin : g_no_sine
        assign sine = 0;
        assign sine_tag = 0;
        assign sine_valid = 0;
    end endgenerate
endmodule
