# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl

source $sdk_path/fpga/lib/starting_point.tcl
#source $sdk_path/fpga/lib/starting_point.tcl

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts

# Connect LEDs to config register
create_bd_port -dir O -from 7 -to 0 led_o
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]

# Add Dual port BRAM for configuration of picoblaze instructions via AXI

cell xilinx.com:ip:blk_mem_gen:8.4 blk_mem_gen_0 {
    use_bram_block Stand_Alone
    Memory_Type True_Dual_Port_RAM
    Enable_32bit_Address false
    Write_Width_A 18
    Write_Depth_A 2048
    Operating_Mode_B READ_FIRST
    Fill_Remaining_Memory_Locations true
} {
    clkb ps_0/FCLK_CLK0
}

cell xilinx.com:ip:axi_bram_ctrl:4.1 axi_bram_ctrl_0 {
    SINGLE_PORT_BRAM 1
    PROTOCOL AXI4LITE
} {
    S_AXI axi_mem_intercon_0/M[add_master_interface]_AXI
    s_axi_aclk ps_0/FCLK_CLK0
    s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
    BRAM_PORTA blk_mem_gen_0/BRAM_PORTA
}

# Add picoblaze core

cell xilinx:user:kcpsm6:1.0 picoblaze {
    hwbuild {"00000001"}
    interrupt_vector 0x3FF
    scratch_pad_memory_size 64
} {
    bram_enable blk_mem_gen_0/enb
    instruction blk_mem_gen_0/doutb
    clk ps_0/FCLK_CLK0
    reset [ctl_pin reset]
    in_port [ctl_pin in_port]
}

# Connect picoblaze address to BRAM

connect_pins blk_mem_gen_0/addrb picoblaze/address

# Register picoblaze out_port

cell xilinx.com:ip:c_shift_ram:12.0 c_shift_ram_0 {
    CE true
    Depth 1
    Width 8
} {
    D picoblaze/out_port
    CLK ps_0/FCLK_CLK0
    CE picoblaze/write_strobe
    Q [sts_pin out_port]
}

assign_bd_address [get_bd_addr_segs {axi_bram_ctrl_0/S_AXI/Mem0 }]
set_property offset [get_memory_offset picoram] [get_bd_addr_segs {ps_0/Data/SEG_axi_bram_ctrl_0_Mem0}]
set_property range [get_memory_range picoram] [get_bd_addr_segs {ps_0/Data/SEG_axi_bram_ctrl_0_Mem0}]
