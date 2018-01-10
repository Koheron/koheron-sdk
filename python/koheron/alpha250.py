#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import command
import numpy as np

class Alpha250(object):
    def __init__(self, client):
        self.client = client

    # Clock generator

    @command(classname='ClockGenerator')
    def set_tcxo_clock(self, value):
        return self.client.recv_int32()

    @command(classname='ClockGenerator')
    def set_tcxo_calibration(self, value):
        return self.client.recv_int32()

    @command(classname='ClockGenerator')
    def set_reference_clock(self, clkin):
        pass

    @command(classname='ClockGenerator')
    def set_sampling_frequency(self, fs_select):
        pass

    # Precision ADC

    @command(classname='PrecisionAdc', funcname='get_device_id')
    def get_precision_adc_id(self):
        return self.client.recv_uint32()

    @command(classname='PrecisionAdc', funcname='get_adc_values')
    def get_precision_adc_values(self):
        return self.client.recv_array(8, dtype='float32')

    # Precision DAC

    @command(classname='PrecisionDac', funcname='set_dac_value')
    def set_precision_dac(self, channel, value):
        pass

    @command(classname='PrecisionDac', funcname='set_dac_value_volts')
    def set_precision_dac_volts(self, channel, voltage):
        pass

    @command(classname='PrecisionDac', funcname='set_calibration_coeffs')
    def set_precision_dac_calibration_coeffs(self, new_coeffs):
        return self.client.recv_int32()

    # RF ADC

    @command(classname='Ltc2157')
    def set_timing(self, delay):
        pass

    @command(classname='Ltc2157', funcname='set_calibration')
    def set_rf_adc_calibration(self, channel, new_coeffs):
        return self.client.recv_int32()

    @command(classname='Ltc2157', funcname='get_calibration')
    def get_rf_adc_calibration(self, channel):
        return self.client.recv_array(8, dtype='float32')

    # GPIO expander

    @command(classname='GpioExpander')
    def set_led(self, value):
        pass

    @command(classname='GpioExpander')
    def set_user_ios(self, value):
        pass

    @command(classname='GpioExpander')
    def get_inputs(self):
        return self.client.recv(fmt='I')

    # Temperature sensor

    @command(classname='TemperatureSensor')
    def get_temperatures(self):
        return self.client.recv_array(3, dtype='float32')

    # Power monitor

    @command(classname='PowerMonitor')
    def get_shunt_voltage(self, index):
        return self.client.recv_float()

    @command(classname='PowerMonitor')
    def get_bus_voltage(self, index):
        return self.client.recv_float()

    # Misc

    @command(classname='Common')
    def get_dna(self):
        return self.client.recv_uint64()

    @command(classname='Eeprom')
    def set_serial_number(self, sn):
        return self.client.recv_int32()

    @command(classname='Eeprom')
    def get_serial_number(self):
        return self.client.recv_uint32()