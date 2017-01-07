# CLK on IO7P
set_property IOSTANDARD LVCMOS33 [get_ports dio*]
set_property SLEW       FAST     [get_ports dio*]
set_property DRIVE      8        [get_ports dio*]

set_property PACKAGE_PIN H16 [get_ports dio1p]
set_property PACKAGE_PIN J18 [get_ports dio2p]
