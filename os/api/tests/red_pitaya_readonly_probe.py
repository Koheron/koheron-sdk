"""Host-side HTTP/TCP probe of a running Red Pitaya FFT, without setters."""
import argparse
import datetime
import json
from pathlib import Path
import socket
import urllib.request

from koheron import KoheronClient


def probe(host):
    socket.setdefaulttimeout(5)
    with urllib.request.urlopen('http://' + host + '/api/instruments/details', timeout=5) as response:
        status = json.load(response)
    client = KoheronClient(host)
    try:
        client.send_command(1, 0)
        version = client.recv_string(check_type=False)

        def read(name):
            driver, command, arguments = client.get_ids('FFT', name)
            assert not arguments
            client.last_device_called, client.last_cmd_called = 'FFT', name
            client.send_command(driver, command)
            return client.recv_uint32()

        size, averages, window = (read(name) for name in ('get_fft_size', 'get_number_averages', 'get_window_index'))
        assert status['live_instrument']['name'] == 'fft'
        assert size == 2048 and averages > 0 and 0 <= window <= 3
        return {'recorded_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'host': host,
            'live_instrument': status['live_instrument'], 'server_version': version, 'fft_size': size,
            'number_averages': averages, 'window_index': window, 'drivers': sorted(client.devices_idx),
            'scope': 'Read-only production HTTP status and TCP RPC reads; no settings changed or FPGA programmed'}
    finally:
        client.sock.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(probe(args.host), indent=2) + '\n')
