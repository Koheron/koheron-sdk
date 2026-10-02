source [file normalize [file join [file dirname [info script]] .. .. red-pitaya-gen2 config board_preset.tcl]]

# Two 4-Gbit x16 devices: 1 GiB over a 32-bit DDR bus.
set_property -dict [list \
    CONFIG.PCW_DDR_RAM_HIGHADDR {0x3FFFFFFF} \
    CONFIG.PCW_UIPARAM_DDR_BUS_WIDTH {32 Bit} \
    CONFIG.PCW_UIPARAM_DDR_DRAM_WIDTH {16 Bits} \
] [get_bd_cells $ps_name]
