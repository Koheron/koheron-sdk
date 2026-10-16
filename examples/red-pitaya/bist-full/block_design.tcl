source $board_path/config/ports.tcl
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl
source $sdk_path/fpga/lib/redp_adc_dac.tcl
add_redp_adc_dac adc_dac
# DNA_PORT requires a period of at least 10 ns.
set_property CONFIG.CLKOUT5_REQUESTED_OUT_FREQ 62.5 [get_bd_cells adc_dac/pll]
cell xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_adc_clk {} {
    ext_reset_in ps_0/FCLK_RESET0_N slowest_sync_clk adc_dac/adc_clk
}
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts adc_dac/adc_clk proc_sys_reset_adc_clk/peripheral_aresetn
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]
cell xilinx.com:ip:xlconcat:2.1 concat_interrupts {NUM_PORTS 5} {dout ps_0/IRQ_F2P}
set adc_clk adc_dac/adc_clk
set rst_adc_clk_name proc_sys_reset_adc_clk
set clk adc_dac/adc_clk
set rst proc_sys_reset_adc_clk/peripheral_aresetn
set_property CONFIG.PCW_USE_S_AXI_HP0 1 [get_bd_cells ps_0]
connect_pins ps_0/S_AXI_HP0_ACLK $clk
source $sdk_path/fpga/lib/bram.tcl
add_bram bram
cell koheron:user:full_path:1.0 path {} {
    aclk $clk aresetn $rst dna_clk adc_dac/pll/clk_out5
    mode [ctl_pin mode] capture [ctl_pin capture] samples [ctl_pin samples]
    snapshot [ctl_pin snapshot] reset_diag [ctl_pin reset_diag] seed [ctl_pin scratch]
    stalls [ctl_pin stalls] fault [ctl_pin fault] packet_bytes [ctl_pin packet_bytes]
    decimation [ctl_pin decimation] irq_period [ctl_pin irq_period] irq_ack [ctl_pin irq_ack]
    irq concat_interrupts/In4
    checker_errors_snapshot [sts_pin checker_errors] keep_errors_snapshot [sts_pin keep_errors]
    input_stalls_snapshot [sts_pin input_stalls] output_stalls_snapshot [sts_pin output_stalls]
    irq_count_snapshot [sts_pin irq_count] irq_pending [sts_pin irq_pending]
    adc0 adc_dac/adc1 adc1 adc_dac/adc2 dac0 adc_dac/dac1 dac1 adc_dac/dac2
    bram_q blk_mem_gen_bram/doutb
    overflow_snapshot [sts_pin overflow] packets_snapshot [sts_pin packets] discontinuities_snapshot [sts_pin discontinuities]
    capture_done [sts_pin capture_done] snapshot_id [sts_pin snapshot_id]
}
foreach {port regname} {cycles_snapshot cycles tx_snapshot tx_words rx_snapshot rx_words dna dna} {
    for {set i 0} {$i < 2} {incr i} {
        connect_pins [sts_pin ${regname}$i] [get_slice_pin path/$port [expr 32*$i+31] [expr 32*$i]]
    }
}
connect_cell blk_mem_gen_bram {
    clkb $clk addrb path/bram_addr dinb path/bram_data web path/bram_we
    enb [get_constant_pin 1 1] rstb [get_constant_pin 0 1]
}
connect_pins [sts_pin identity] [get_constant_pin 0x42494631 32]
connect_pins [sts_pin scratch_echo] [ctl_pin scratch]
set_property CONFIG.NUM_PORTS 5 [get_bd_cells concat_interrupts]
for {set i 0} {$i < 2} {incr i} {
    set name uart_[lindex {a b} $i]
    set idx [add_master_interface 0]
    cell koheron:user:bist_uart:1.0 $name {FIFO_BITS 6} {
        aclk $clk aresetn $rst
    }
    # Registers cross from PS to the ADC clock through a standard AXI converter.
    cell xilinx.com:ip:axi_clock_converter:2.1 cc_$name {} {
        s_axi_aclk ps_0/FCLK_CLK0 s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
        m_axi_aclk $clk m_axi_aresetn $rst
        S_AXI axi_mem_intercon_0/M${idx}_AXI M_AXI $name/S_AXI
    }
    connect_pins $name/irq concat_interrupts/In$i
    assign_bd_address [get_bd_addr_segs $name/s_axi/reg0]
    set seg [get_bd_addr_segs ps_0/Data/SEG_${name}_reg0]
    set_property offset [get_memory_offset $name] $seg
    set_property range [get_memory_range $name] $seg
}
connect_pins uart_a/tx uart_b/rx
connect_pins uart_b/tx uart_a/rx
cell xilinx.com:ip:util_vector_logic:2.0 dma_irq {C_SIZE 1 C_OPERATION or} {Res concat_interrupts/In2}

set idx [add_master_interface 0]
cell xilinx.com:ip:axi_dma:7.1 dma {
    c_include_mm2s_dre 1 c_include_s2mm_dre 1
    c_include_sg 1 c_sg_length_width 23 c_sg_include_stscntrl_strm 0
    c_m_axi_mm2s_data_width 64 c_m_axi_s2mm_data_width 64
    c_m_axis_mm2s_tdata_width 32 c_s_axis_s2mm_tdata_width 32
    c_mm2s_burst_size 64 c_s2mm_burst_size 64
} {
    s_axi_lite_aclk ps_0/FCLK_CLK0 axi_resetn proc_sys_reset_0/peripheral_aresetn
    m_axi_mm2s_aclk $clk m_axi_s2mm_aclk $clk m_axi_sg_aclk $clk
    S_AXI_LITE axi_mem_intercon_0/M${idx}_AXI
    M_AXIS_MM2S path/S_AXIS
    mm2s_introut dma_irq/Op1 s2mm_introut dma_irq/Op2
}
cell xilinx.com:ip:proc_sys_reset:5.0 capture_reset {} {
    slowest_sync_clk $clk ext_reset_in path/data_resetn
}
cell xilinx.com:ip:axis_data_fifo:2.0 capture_fifo {
    TDATA_NUM_BYTES 4 FIFO_DEPTH 512 HAS_TKEEP 1 HAS_TLAST 1
} {
    s_axis_aclk $clk s_axis_aresetn capture_reset/peripheral_aresetn S_AXIS path/M_AXIS M_AXIS dma/S_AXIS_S2MM
}
cell xilinx.com:ip:axi_interconnect:2.1 hp {
    NUM_SI 3 NUM_MI 1
} {
    ACLK $clk ARESETN $rst S00_ACLK $clk S01_ACLK $clk S02_ACLK $clk
    M00_ACLK $clk S00_ARESETN $rst S01_ARESETN $rst S02_ARESETN $rst M00_ARESETN $rst
    S00_AXI dma/M_AXI_MM2S S01_AXI dma/M_AXI_S2MM S02_AXI dma/M_AXI_SG
    M00_AXI ps_0/S_AXI_HP0
}
assign_bd_address [get_bd_addr_segs dma/S_AXI_LITE/Reg]
set seg [get_bd_addr_segs ps_0/Data/SEG_dma_Reg]
set_property offset [get_memory_offset dma] $seg
set_property range [get_memory_range dma] $seg
foreach space {Data_MM2S Data_S2MM Data_SG} {
    assign_bd_address -target_address_space dma/$space [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_DDR_LOWOCM] -force
}

# The board's auxiliary inputs use the dedicated XADC; keep its IRQ separate
# from the UART IRQs and map its registers through the PS clock interconnect.
set idx [add_master_interface 0]
cell xilinx.com:ip:xadc_wiz:3.3 xadc {
    XADC_STARUP_SELECTION independent_adc
    CHANNEL_ENABLE_VAUXP0_VAUXN0 true CHANNEL_ENABLE_VAUXP1_VAUXN1 true
    CHANNEL_ENABLE_VAUXP8_VAUXN8 true CHANNEL_ENABLE_VAUXP9_VAUXN9 true
    CHANNEL_ENABLE_VP_VN true DCLK_FREQUENCY 125 ADC_CONVERSION_RATE 1000
} {
    s_axi_aclk ps_0/FCLK_CLK0 s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
    ip2intc_irpt concat_interrupts/In3 s_axi_lite axi_mem_intercon_0/M${idx}_AXI
}
foreach port {Vp_Vn Vaux0 Vaux1 Vaux8 Vaux9} {
    connect_bd_intf_net [get_bd_intf_pins xadc/$port] [get_bd_intf_ports $port]
}
assign_bd_address [get_bd_addr_segs xadc/s_axi_lite/Reg]
set seg [get_bd_addr_segs ps_0/Data/SEG_xadc_Reg]
set_property offset [get_memory_offset xadc] $seg
set_property range [get_memory_range xadc] $seg
