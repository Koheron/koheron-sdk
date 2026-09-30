#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import command

class Decimator(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_fft_window(self, window_index):
        pass

    @command()
    def get_control_parameters(self):
        return self.client.recv_tuple('ffffIII')

    @command()
    def spectral_density0(self):
        return self.client.recv_vector(dtype='float64')

    @command()
    def spectral_density1(self):
        return self.client.recv_vector(dtype='float64')
