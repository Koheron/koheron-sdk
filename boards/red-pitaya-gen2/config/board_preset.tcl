# Gen2 keeps the original ADC/DAC and PS peripheral wiring.
source [file normalize [file join [file dirname [info script]] .. .. red-pitaya config board_preset.tcl]]

# MIO46 selects the SD/eMMC switch; MIO47 is not a write-protect input.
# Leave SD selection at its board pull-up. QSPI/eMMC need an optional E3 module.
set_property -dict [list \
    CONFIG.PCW_DDR_RAM_HIGHADDR {0x1FFFFFFF} \
    CONFIG.PCW_UIPARAM_DDR_BUS_WIDTH {16 Bit} \
    CONFIG.PCW_UIPARAM_DDR_DRAM_WIDTH {16 Bits} \
    CONFIG.PCW_SD0_GRP_CD_ENABLE {0} \
    CONFIG.PCW_SD0_GRP_CD_IO {<Select>} \
    CONFIG.PCW_SD0_GRP_WP_ENABLE {0} \
    CONFIG.PCW_SD0_GRP_WP_IO {<Select>} \
    CONFIG.PCW_EN_QSPI {0} \
    CONFIG.PCW_QSPI_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_QSPI_GRP_SINGLE_SS_ENABLE {0} \
    CONFIG.PCW_MIO_46_DIRECTION {in} \
    CONFIG.PCW_MIO_46_PULLUP {enabled} \
] [get_bd_cells $ps_name]
