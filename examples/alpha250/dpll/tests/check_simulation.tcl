# XSim can stop on $fatal without propagating a failing Vivado exit status.
proc check_simulation {filename marker} {
    set handle [open $filename r]
    set result [read $handle]
    close $handle
    if {[string first $marker $result] < 0 || [regexp {Fatal:|FATAL:|ERROR:} $result]} {
        error "Simulation did not pass: $filename"
    }
}
