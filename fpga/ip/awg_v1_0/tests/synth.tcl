set root [file normalize [file join [file dirname [info script]] ..]]
file mkdir tmp/dds-pm-resources
set profiles [dict create full {} basic {ENABLE_GAUSSIAN=0 ENABLE_UNIFORM=0 ENABLE_PRBS=0} \
    tone {ENABLE_SINE=0 ENABLE_SQUARE=0 ENABLE_PULSE=0 ENABLE_TRIANGLE=0 ENABLE_UP_RAMP=0 ENABLE_DOWN_RAMP=0 ENABLE_UNIFORM=0 ENABLE_GAUSSIAN=0 ENABLE_PRBS=0 ENABLE_BPSK=0}]
dict for {name parameters} $profiles {
    create_project -in_memory -part xc7z020clg400-2
    add_files [glob $root/*.v]
    synth_design -top awg -mode out_of_context -part xc7z020clg400-2 -generic $parameters
    report_utilization -file tmp/dds-pm-resources/$name.txt
    close_project
}
