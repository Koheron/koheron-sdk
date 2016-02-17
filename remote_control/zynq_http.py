#! /usr/bin/python

import requests
import os

class ZynqHTTP:
    def __init__(self, IP, port=80):
        self.url = 'http://' + IP + ':' + str(port)

    def get_bistream_id(self):
        r = requests.get(self.url + '/bitstream_id')
        return r.text

    def ping(self):
        r = requests.post(self.url + '/ping', data={})

    def deploy_remote_instrument(self, name, version):
        """ Deploy a remotely available instrument

            Args:
                - name: Instrument name
                - version: Instrument version
        """
        zip_filename = name + '-' + version + '.zip'
        r = requests.post(self.url + '/deploy/remote/' + zip_filename, data={})

    def deploy_local_instrument(self, name, version):
        zip_filename = name + '-' + version + '.zip'
        print('Deploying ' + zip_filename)
        try:
            r = requests.post(self.url + '/deploy/local/' + zip_filename, data={} , timeout=0.5)
        except:
            pass
            #print('Timeout occured')

    def remove_local_instrument(self, name, version):
        zip_filename = name + '-' + version + '.zip'
        r = requests.get(self.url + '/remove/local/' + zip_filename)
        return r.text

    def get_local_instruments(self):
        try:
            r = requests.get(self.url + '/get_local_instruments')
            return r.json()
        except:
            return {}

    def upload_instrument(self, zipfilename):
        try:
            files = {}
            files[os.path.basename(zipfilename)] = open(zipfilename, 'rb')
            r = requests.post(self.url + '/upload/instrument_zip', files=files)
        except:
            print("Upload failed")

    def upload_and_install_instrument(self, zipfilename):
        self.upload_instrument(zipfilename)

        print('Deploying ' + zipfilename)
        try:
            r = requests.post(self.url + '/deploy/local/' + os.path.basename(zipfilename),
                              data={} , timeout=0.5)
        except:
            print('Timeout occured')
            pass

    def install_instrument(self, instrument_name):
        instruments = self.get_local_instruments()
        if instruments:
            for name, shas in instruments.items():
                if name == instrument_name and len(shas) > 0:
                    self.deploy_local_instrument(name, shas[0])
                    return
        raise ValueError("Instrument " + instrument_name + " not found")

if __name__ == "__main__":
    http = ZynqHTTP('192.168.1.4')
    print(http.get_bistream_id())
#    http.ping()
#    http.deploy_remote_instrument('spectrum', '06ee48f')
#    http.deploy_local_instrument('oscillo', '06ee48f')
#    print(http.remove_local_instrument('oscillo', '06ee48f'))
#    print(http.get_local_instruments())
#    http.install_instrument("spectrum")
    http.upload_and_install_instrument('../pmod_modules-be286c9.zip')
