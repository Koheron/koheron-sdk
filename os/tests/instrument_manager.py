#! /usr/bin/python

import requests
import time
import hashlib

class InstrumentManager:
    def __init__(self, ip, port=80):
        self.port = port
        self.url = 'http://' + ip + ':' + str(port)

    def set_ip(self, ip):
        self.url = 'http://' + ip + ':' + str(self.port)

    def get_bitstream_id(self):
        r = requests.get(self.url + '/api/board/bitstream_id')
        return r.text

    def get_dna(self):
        r = requests.get(self.url + '/api/board/dna')
        return r.text

    def ping(self):
        r = requests.get(self.url + '/api/board/ping')

    def get_app_version(self):
        try:
            r = requests.get(self.url + '/api/app/version')
            return r.json()
        except Exception as e:
            print("[error] " + str(e))
            return {}

    def get_board_version(self):
        try:
            r = requests.get(self.url + '/api/board/version')
            return r.json()
        except Exception as e:
            print("[error] " + str(e))
            return {}
        
    def deploy_local_instrument(self, name, version):
        """ Deploy a locally available instrument
            Return 0 on success, -1 else
        """
        print('Deploying instrument {} with version {}'.format(name, version))
        try:
            r = requests.get('{}/api/instruments/run/{}/{}'.format(self.url, name, version))
            return int(r.text.split('status:')[1].strip())
        except Exception as e: 
            print("[error] " + str(e))
            return -1

    def remove_local_instrument(self, name, version):
        r = requests.get('{}/api/instruments/delete/{}/{}'.format(self.url, name, version))
        return r.text

    def get_local_instruments(self):
        try:
            r = requests.get(self.url + '/api/instruments/local')
            return r.json()
        except Exception as e: 
            print("[error] " + str(e))
            return {}

    def get_live_instrument(self):
        try:
            r = requests.get(self.url + '/api/instruments/live')
            return r.json()
        except Exception as e: 
            print("[error] " + str(e))
            return {}

    def get_server_version(self):
        live_instrum = self.get_live_instrument()
        return live_instrum['server_version']

    def install_instrument(self, instrument_name, always_restart=False):
        if not always_restart:
            # Don't restart the instrument if already launched
            live_instrument = self.get_live_instrument()
            if live_instrument['name'] == instrument_name:
                return

        instruments = self.get_local_instruments()
        if instruments:
            for name, shas in instruments.items():
                if name == instrument_name and len(shas) > 0:
                    if self.deploy_local_instrument(name, shas[0]) < 0:
                        raise RuntimeError("Instrument " + instrument_name + " launch failed.")
                    return
        raise ValueError("Instrument " + instrument_name + " not found")

    def restore_backup(self):
        r = requests.get(self.url + '/api/instruments/restore')
        return r.text

