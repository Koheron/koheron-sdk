set sdk_path .
set width [lindex $argv 1]
set size [lindex $argv 2]
set frequency [lindex $argv 3]
proc get_parameter {name} {
 switch $name {
  adc_width {return $::width}
  fft_size {return $::size}
  adc_clk {return $::frequency}
 }
}
proc current_bd_instance {args} {return .}
proc create_bd_cell {args} {return [lindex $args end]}
proc create_bd_pin {args} {puts [list pin {*}$args]}
proc add_bram {args} {return demod_bram}
proc connect_cell {name ports} {puts [list connect $name [dict create {*}[uplevel 1 [list subst $ports]]]]}
proc cell {ip name props ports} {
 puts [list cell $ip $name [dict create {*}[uplevel 1 [list subst $props]]] [dict create {*}[uplevel 1 [list subst $ports]]]]
}
foreach command {get_constant_pin get_Q_pin get_concat_pin get_slice_pin get_and_pin} {
 proc $command {args} {return [string map {" " _ "\n" ""} [list [lindex [info level 0] 0] {*}$args]]}
}
source [lindex $argv 0]
if {[llength $argv] > 4} {power_spectral_density::create psd $size [lindex $argv 4]} else {power_spectral_density::create psd $size}
