
# AXI Stream on XADC
set_property -dict [list CONFIG.ENABLE_AXI4STREAM {true}] [get_bd_cells xadc]
connect_pins xadc/s_axis_aclk adc_dac/adc_clk
connect_pins xadc/m_axis_tready [get_constant_pin 1 1]

    # Start laser controller
    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier laser_controller]

    create_bd_pin -dir I -type clk clk
    create_bd_pin -dir I rst

    create_bd_pin -dir I -from 31 -to 0 s_axis_tdata
    create_bd_pin -dir I -from 4 -to 0 s_axis_tid
    create_bd_pin -dir I s_axis_tvalid

    create_bd_pin -dir I -from 31 -to 0 laser_current
    create_bd_pin -dir I -from 31 -to 0 laser_control
    create_bd_pin -dir I -from 31 -to 0 power_setpoint

    create_bd_pin -dir O -from 11 -to 0 pid_control
    create_bd_pin -dir O -from 3 -to 0 pwm

    create_bd_pin -dir O laser_shutdown
    create_bd_pin -dir O laser_reset_overvoltage

    create_bd_pin -dir I -from 31 -to 0 eeprom_ctl
    create_bd_pin -dir O -from 31 -to 0 eeprom_sts

    create_bd_pin -dir I laser_eeprom_dout
    create_bd_pin -dir O laser_eeprom_din
    create_bd_pin -dir O laser_eeprom_sclk
    create_bd_pin -dir O laser_eeprom_cs

    # Add pulse density modulator for laser current control
    cell koheron:user:pdm:2.0 laser_current_pdm {
        NBITS [get_parameter pwm_width]
    } {
        clk clk
        rst rst
    }

    connect_pin pwm [get_concat_pin [list [get_constant_pin 0 3] laser_current_pdm/dout]]
    connect_pin laser_shutdown [get_slice_pin laser_control 0 0]
    connect_pin laser_reset_overvoltage [get_slice_pin laser_control 1 1]

    set clk_en [get_and_pin s_axis_tvalid [get_EQ_pin s_axis_tid [get_constant_pin 17 5]] "clock_enable"]

    cell xilinx.com:ip:c_addsub:12.0 error {
        A_Type Signed
        B_Type Signed
        A_Width 16
        B_Width 16
        Add_Mode Subtract
    } {
        A power_setpoint
        B s_axis_tdata
        CLK clk
        CE $clk_en
    }

    cell xilinx.com:ip:c_shift_ram:12.0 shift_reg {
        Depth 1
        Width 1
        } {
        D $clk_en
        CLK clk
    }

    cell xilinx.com:ip:c_accum:12.0 integrator {
        Input_Width 16
        Output_Width 32
        CE true
    } {
        B error/S
        CLK clk
        CE shift_reg/Q
        BYPASS [get_slice_pin laser_control 0 0]
    }

    cell koheron:user:saturation:1.0 saturation {
        DATA_WIDTH 24
        MAX_VAL 4095
    } {
        clk clk
        din [get_slice_pin integrator/Q 31 8]
    }

    cell koheron:user:bus_multiplexer:1.0 mux {
        WIDTH 12
    } {
        din0 laser_current
        din1 [get_slice_pin saturation/dout 11 0]
        sel [get_slice_pin laser_control 2 2]
        dout laser_current_pdm/din
        dout pid_control
    }

    cell koheron:user:at93c46d_spi:1.0 eeprom {} {
        clk clk
        cmd [get_slice_pin eeprom_ctl 8 1]
        start [get_slice_pin eeprom_ctl 0 0]
        dout laser_eeprom_dout
        din laser_eeprom_din
        cs laser_eeprom_cs
        sclk laser_eeprom_sclk
        data_in [get_slice_pin eeprom_ctl 31 16]
        data_out eeprom_sts
    }

    current_bd_instance $bd
    # End laser_controller

# Connect laser shutdown pin and reset overvoltage protection
create_bd_port -dir O laser_shutdown
create_bd_port -dir O laser_reset_overvoltage

# EEPROM SPI port
create_bd_port -dir I laser_eeprom_dout
create_bd_port -dir O laser_eeprom_din
create_bd_port -dir O laser_eeprom_sclk
create_bd_port -dir O laser_eeprom_cs

connect_cell laser_controller {
    clk adc_dac/adc_clk
    rst proc_sys_reset_adc_clk/peripheral_reset
    s_axis_tdata xadc/m_axis_tdata
    s_axis_tvalid xadc/m_axis_tvalid
    s_axis_tid xadc/m_axis_tid
    laser_current [ctl_pin laser_current]
    laser_control [ctl_pin laser_control]
    power_setpoint [ctl_pin power_setpoint]
    pid_control [sts_pin pid_control]
    pwm dac_pwm_o
    laser_shutdown laser_shutdown
    laser_reset_overvoltage laser_reset_overvoltage
    eeprom_ctl [ctl_pin eeprom_ctl]
    eeprom_sts [sts_pin eeprom_sts]
    laser_eeprom_dout laser_eeprom_dout
    laser_eeprom_din laser_eeprom_din
    laser_eeprom_sclk laser_eeprom_sclk
    laser_eeprom_cs laser_eeprom_cs
}