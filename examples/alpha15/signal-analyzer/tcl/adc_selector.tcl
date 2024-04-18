namespace eval adc_selector {

proc create {module_name} {
    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier adc_selector]
    create_bd_pin -dir I -from 18  -to 0 adc0
    create_bd_pin -dir I -from 18  -to 0 adc1
    create_bd_pin -dir I -from 18  -to 0 offset0
    create_bd_pin -dir I -from 18  -to 0 offset1
    create_bd_pin -dir I -from 18  -to 0 channel_select
    create_bd_pin -dir O -from 19  -to 0 tdata
    create_bd_pin -dir I -type clk       clk
    create_bd_pin -dir I                 adc_valid
    create_bd_pin -dir O                 tvalid

    for {set i 0} {$i < 2} {incr i} {
        cell koheron:user:bus_multiplexer:1.0 adc_mux${i} {
            WIDTH 18
        } {
            din0 adc${i}
            din1 offset${i}
            sel  [get_slice_pin channel_select ${i} ${i}]
        }
    }

    cell xilinx.com:ip:c_addsub:12.0 adc_addsub {
        A_WIDTH 18
        B_WIDTH 18
        OUT_WIDTH 19
        ADD_MODE Add_Subtract
        CE false
    } {
        A   adc_mux0/dout
        B   adc_mux1/dout
        ADD [get_slice_pin channel_select 2 2]
        CLK clk
        S   tdata
    }

    # Latency of 1 in addsub ip
    connect_pins tvalid [get_Q_pin adc_valid 1]

    current_bd_instance $bd
}

} ;# end adc_selector namespace