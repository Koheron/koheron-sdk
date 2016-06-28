source projects/oscillo/config.tcl

set bram_size [expr 2**($config::bram_addr_width-8)]K

source projects/base/block_design.tcl

source projects/oscillo/oscillo.tcl

source projects/base/counter.tcl

##########################################################
# Add EEPROM
##########################################################

source $lib/at93c46d.tcl
