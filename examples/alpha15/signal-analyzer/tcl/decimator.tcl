namespace eval decimator {

proc pins {cmd} {
    $cmd -dir I -from 18  -to 0 tdata
    $cmd -dir I -from 0   -to 0 tvalid
    $cmd -dir I -from 0   -to 0 s_axis_aresetn
    $cmd -dir I -from 0   -to 0 m_axis_aresetn
    $cmd -dir I -from 15   -to 0 rate
    $cmd -dir O -from 0   -to 0 interrupt
    $cmd -dir I -type clk       clk
    $cmd -dir I -type clk       ps_clk
}

proc create {module_name fir_coeffs} {
    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier $module_name]

    pins create_bd_pin
    create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S_AXI

    # Define CIC/FIR parameters
    set diff_delay [get_parameter cic_differential_delay]
    set dec_rate_default [get_parameter cic_decimation_rate_default]
    set dec_rate_min [get_parameter cic_decimation_rate_min]
    set dec_rate_max [get_parameter cic_decimation_rate_max]
    set n_stages [get_parameter cic_n_stages]

    # Use AXI Stream clock converter (ADC clock -> PS clock)
    cell xilinx.com:ip:axis_clock_converter:1.1 adc_clock_converter {
        TDATA_NUM_BYTES 3
    } {
        s_axis_tdata   tdata
        s_axis_tvalid  tvalid
        s_axis_aresetn s_axis_aresetn
        m_axis_aresetn m_axis_aresetn
        s_axis_aclk    clk
        m_axis_aclk    ps_clk
    }

    cell xilinx.com:ip:cic_compiler:4.0 cic {
        Filter_Type Decimation
        Number_Of_Stages $n_stages
        Fixed_Or_Initial_Rate $dec_rate_default
        Sample_Rate_Changes Programmable
        Minimum_Rate $dec_rate_min
        Maximum_Rate $dec_rate_max
        Differential_Delay $diff_delay
        Input_Sample_Frequency 15
        Clock_Frequency [expr [get_parameter fclk0] / 1000000.]
        Input_Data_Width [get_parameter adc_width]
        Quantization Truncation
        Output_Data_Width 32
        Use_Xtreme_DSP_Slice false
    } {
        aclk        ps_clk
        S_AXIS_DATA adc_clock_converter/M_AXIS
    }

    cell pavel-demin:user:axis_variable:1.0 cic_rate {
        AXIS_TDATA_WIDTH 16
    } {
        cfg_data rate
        aclk ps_clk
        aresetn m_axis_aresetn
        M_AXIS cic/S_AXIS_CONFIG
    }

    cell xilinx.com:ip:fir_compiler:7.2 fir {
        Filter_Type Decimation
        Sample_Frequency [expr 15.0 / $dec_rate_min]
        Clock_Frequency [expr [get_parameter fclk0] / 1000000.]
        Coefficient_Width 32
        Data_Width 32
        Output_Rounding_Mode Convergent_Rounding_to_Even
        Output_Width 32
        Decimation_Rate 2
        BestPrecision true
        CoefficientVector [subst {{$fir_coeffs}}]
    } {
        aclk ps_clk
        S_AXIS_DATA cic/M_AXIS_DATA
    }

    # Add AXI stream FIFO
    cell xilinx.com:ip:axi_fifo_mm_s:4.3 adc_axis_fifo {
        C_USE_TX_DATA 0
        C_USE_TX_CTRL 0
        C_USE_RX_CUT_THROUGH true
        C_RX_FIFO_DEPTH [get_parameter fifo_depth]
        C_RX_FIFO_PF_THRESHOLD [expr [get_parameter fifo_depth] / 2]
    } {
        s_axi_aclk    ps_clk
        s_axi_aresetn m_axis_aresetn
        AXI_STR_RXD   fir/M_AXIS_DATA
    }

    connect_bd_intf_net [get_bd_intf_pins S_AXI] [get_bd_intf_pins adc_axis_fifo/S_AXI]
    connect_pins adc_axis_fifo/interrupt interrupt
    current_bd_instance $bd
}

}