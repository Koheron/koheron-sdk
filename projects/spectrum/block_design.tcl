source projects/spectrum/config.tcl

set bram_size [expr 2**($bram_addr_width-8)]K

source projects/base/block_design.tcl

source projects/spectrum/spectrum.tcl

##########################################################
# Add EEPROM
##########################################################

source lib/at93c46d.tcl
