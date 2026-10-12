import os
from koheron import connect, command


class VCO:
    def __init__(self, client):
        self.client = client

    @command()
    def set_dds_freq(self, channel, freq):
        pass

    @command()
    def set_vco_gain(self, channel, gain):
        pass

    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass


def main():
    host = os.getenv('HOST', '192.168.1.100')
    driver = VCO(connect(host, 'vco', restart=False))

    # 2 = onboard TCXO; use 0 with an external reference connected.
    driver.set_reference_clock(2)

    # Channels are 0 and 1. Keep the sampling clock at 250 MS/s.
    # Gain is a right shift: 0 = maximum sensitivity, 48 = minimum.
    driver.set_vco_gain(0, 4)
    driver.set_dds_freq(0, 10e6)


if __name__ == '__main__':
    main()
