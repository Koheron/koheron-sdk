
set bram_size [expr 2**($config::bram_addr_width-8)]K

source projects/base/block_design.tcl

source projects/oscillo/oscillo.tcl

##########################################################
# Add EEPROM
##########################################################

source $lib/at93c46d.tcl
