source projects/oscillo/config.tcl

set bram_size [expr 2**($bram_addr_width-8)]K

source projects/base/block_design.tcl

source projects/oscillo/oscillo.tcl

##########################################################
# Add EEPROM
##########################################################

source projects/base/at93c46d.tcl
