# Scoped to each awg instance. The source mailbox stays unchanged until the
# destination captures it and returns an acknowledgement. Bound its data delay
# to 8 ns for sample clocks up to 250 MHz. Two request synchronizers plus a
# registered capture enable leave at least three periods (12 ns at 250 MHz)
# for data settling. Slower clocks, including Red Pitaya, have more margin.
set destination [get_cells -hier -filter {IS_SEQUENTIAL && (NAME =~ *active_reg* || NAME =~ *restart_carrier_reg* || NAME =~ *restart_mod_reg*)}]
set source [get_cells -hier -filter {IS_SEQUENTIAL && NAME =~ *mailbox_reg*}]
set_max_delay -datapath_only 8.0 -from $source -to $destination
set_bus_skew 4.0 -from $source -to $destination
# Data remains stable until acknowledgement has crossed back to AXI; this is
# a bundled-data asynchronous transfer, not an edge-related hold relationship.
set_false_path -hold -from $source -to $destination
set synchronizers [get_cells -hier -regexp {.*(request_sync|ack_sync)_reg\[0\]}]
set pins [get_pins -of_objects $synchronizers -filter {REF_PIN_NAME == D}]
set_false_path -to $pins
set resets [get_pins -hier -filter {NAME =~ *reset_sync_reg*/CLR || NAME =~ *sample_resetn_reg*/CLR}]
set_false_path -to $resets
