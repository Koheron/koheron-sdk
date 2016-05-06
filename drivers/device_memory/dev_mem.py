# dev_mem.py
# Client API for the DEVICE_MEMORY device

import numpy as np
from .kclient import command, write_buffer

class DevMem:
    def __init__(self, client):
        self.client = client

    @command('DEVICE_MEMORY')
    def add_memory_map(self, device_addr, map_size):
        """ Add a memory map

        Args:
            device_addr: Physical address of the device
            map_size: Mmap size

        Return:
            The ID of the memory map
        """
        map_id = self.client.recv_int(4)

        if map_id < 0:
            print("Can't open memory map")

        return map_id

    @command('DEVICE_MEMORY')
    def read(self, mmap_idx, offset):
        """ Read a register

        Args:
            mmap_idx: Index of Memory Map
            offset: Offset of the register to read

        Return:
            The value of the register (integer)
        """
        return self.client.recv_int(4)

    @command('DEVICE_MEMORY')
    def write(self, mmap_idx, offset, reg_val):
        """ Write a register

        Args:
            mmap_idx: Index of Memory Map
            offset: Offset of the register to read
            reg_val: Value to write in the register
        """
        pass

    def write_buffer(self, mmap_idx, offset, data, format_char='I', dtype=np.uint32):
        """ Write a buffer of registers

        Args:
            mmap_idx: Index of Memory Map
            offset: Offset of the register to write
        """
        @write_buffer('DEVICE_MEMORY', format_char=format_char, dtype=dtype)
        def write_buffer(self, data, mmap_idx, offset): pass

        write_buffer(self, data, mmap_idx, offset)

    @command('DEVICE_MEMORY')
    def read_buffer(self, mmap_idx, offset, buff_size, data_type='uint32'):
        """ Read a buffer of registers

        Args:
            mmap_idx: Index of Memory Map
            offset: Offset of the register to read
            buff_size: Number of registers to read
            data_type: Type of received data

        Returns:
            A Numpy array with the data on success.
        """
        return self.client.recv_buffer(buff_size, data_type)

    @command('DEVICE_MEMORY')
    def set_bit(self, mmap_idx, offset, index):
        """ Set a bit (bit = 1)

        Args:
            mmap_idx: Index of Memory Map
            offset: Offset of the register
            index: Index of the bit to set in the register
        """
        pass

    @command('DEVICE_MEMORY')
    def clear_bit(self, mmap_idx, offset, index):
        """ Clear a bit (bit = 0)

        Args:
            mmap_idx: Index of Memory Map
            offset: Offset of the register
            index: Index of the bit to cleared in the register
        """
        pass

    @command('DEVICE_MEMORY')
    def toggle_bit(self, mmap_idx, offset, index):
        """ Toggle the value of a bit

        Args:
            mmap_idx: Index of Memory Map
            offset: Offset of the register
            index: Index of the bit to set in the register
        """
        pass

    @command('DEVICE_MEMORY')
    def mask_and(self, mmap_idx, offset, mask): pass

    @command('DEVICE_MEMORY')
    def mask_or(self, mmap_idx, offset, mask): pass