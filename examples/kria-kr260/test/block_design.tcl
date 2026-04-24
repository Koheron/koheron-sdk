set ps_name ps_0
create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.5 $ps_name

source $board_path/config/board_preset.tcl

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0
apply_bd_automation -rule xilinx.com:bd_rule:board -config { Manual_Source {/ps_0/pl_resetn0 (ACTIVE_LOW)}}  [get_bd_pins proc_sys_reset_0/ext_reset_in]
apply_bd_automation -rule xilinx.com:bd_rule:clkrst -config { Clk {/ps_0/pl_clk0 (99 MHz)} Freq {99} Ref_Clk0 {None} Ref_Clk1 {None} Ref_Clk2 {None}}  [get_bd_pins proc_sys_reset_0/slowest_sync_clk]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_0
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/ps_0/pl_clk0 (99 MHz)} Clk_slave {/ps_0/pl_clk0 (99 MHz)} Clk_xbar {/ps_0/pl_clk0 (99 MHz)} Master {/ps_0/M_AXI_HPM0_FPD} Slave {/axi_gpio_0/S_AXI} ddr_seg {Auto} intc_ip {New AXI SmartConnect} master_apm {0}}  [get_bd_intf_pins axi_gpio_0/S_AXI]

set_property range [get_memory_range gpio] [get_bd_addr_segs {ps_0/Data/SEG_axi_gpio_0_Reg}]
set_property offset [get_memory_offset gpio] [get_bd_addr_segs {ps_0/Data/SEG_axi_gpio_0_Reg}]

create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilslice:1.0 ilslice_0
set_property CONFIG.DIN_WIDTH {3} [get_bd_cells ilslice_0]
connect_bd_net [get_bd_pins ps_0/emio_ttc0_wave_o] [get_bd_pins ilslice_0/Din]
create_bd_port -dir O fan_en_b
connect_bd_net [get_bd_ports fan_en_b] [get_bd_pins ilslice_0/Dout]

create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_irq
set_property -dict [list CONFIG.NUM_PORTS {16}] [get_bd_cells xlconcat_irq]

# Add 16 x UART Lite cores on PMODs
for {set i 0} {$i < 16} {incr i} {
    set uart_name [format "axi_uartlite_%d" $i]
    create_bd_cell -type ip -vlnv xilinx.com:ip:axi_uartlite:2.0 $uart_name
    set_property -dict [list \
        CONFIG.C_BAUDRATE {115200} \
        CONFIG.C_DATA_BITS {8} \
        CONFIG.C_USE_PARITY {0}] [get_bd_cells $uart_name]

    apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [format { \
        Clk_master {/ps_0/pl_clk0 (99 MHz)} \
        Clk_slave {/ps_0/pl_clk0 (99 MHz)} \
        Clk_xbar {/ps_0/pl_clk0 (99 MHz)} \
        Master {/ps_0/M_AXI_HPM0_FPD} \
        Slave {/%s/S_AXI} \
        ddr_seg {Auto} \
        intc_ip {New AXI SmartConnect} \
        master_apm {0}} $uart_name] [get_bd_intf_pins $uart_name/S_AXI]

    set tx_port [create_bd_port -dir O [format "pmod_uart_tx_%02d" $i]]
    set rx_port [create_bd_port -dir I [format "pmod_uart_rx_%02d" $i]]
    connect_bd_net [get_bd_ports [format "pmod_uart_tx_%02d" $i]] [get_bd_pins $uart_name/tx]
    connect_bd_net [get_bd_ports [format "pmod_uart_rx_%02d" $i]] [get_bd_pins $uart_name/rx]

    connect_bd_net [get_bd_pins $uart_name/interrupt] [get_bd_pins [format "xlconcat_irq/In%d" $i]]
}

connect_bd_net [get_bd_pins xlconcat_irq/dout] [get_bd_pins ps_0/pl_ps_irq0]

