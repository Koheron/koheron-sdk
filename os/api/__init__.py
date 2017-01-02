import os
import sys
import shutil
import urllib
import json
import subprocess
import time
import glob
import hashlib
import yaml
from distutils.dir_util import copy_tree

from flask import Flask, render_template, request, url_for
from koheron import KoheronClient, Common

def log(severity, message):
    print("[" + severity + "] " + message)

class KoheronAPIApp(Flask):
    def __init__(self, *args, **kwargs):
        super(KoheronAPIApp, self).__init__(*args, **kwargs)

        self.config['INSTRUMENTS_DIR'] = '/usr/local/instruments/'
        self.instruments = {}

        self.load_metadata()
        self.live_instrument = {'name': None, 'sha': None}
        self.start_last_deployed_instrument()
        self.get_instruments()

    def load_metadata(self):
        self.metadata = {}
        with open('metadata.json', 'r') as f:
            self.metadata = json.load(f)

    def unzip_static(self):
        if os.path.exists('/tmp/static'):
            shutil.rmtree('/tmp/static')
        subprocess.call(['/usr/bin/unzip', '-o', '/tmp/static.zip', '-d', '/tmp/static'])

    def update_live(self):
        if os.path.exists('/tmp/instrument'):
            shutil.rmtree('/tmp/instrument')
        subprocess.call(['/usr/bin/unzip', '-o', '/tmp/live.zip', '-d', '/tmp/instrument'])

    def copy_static(self):
        copy_tree('/tmp/static/ui', '/var/www/ui')

    # ------------------------
    # koheron-server client
    # ------------------------

    def start_client(self):
        log('info', 'Connecting to server...')
        self.client = KoheronClient(unixsock='/var/run/koheron-server.sock')

        if self.client.is_connected:
            log('info', 'Connected to server')
            self.common = Common(self.client)
            log('info', 'Common driver initialized')
            self.common.init()
        else:
            log('error', 'Failed to connect to server')

    def stop_client(self):
        if hasattr(self, 'client'):
            self.client.__del__()

    def ping(self):
        val = self.common.get_led()
        for i in range(255):
            time.sleep(0.01)
            self.common.set_led(i)
        self.common.set_led(val)

    def init(self):
        self.common.init()

    # ------------------------
    # Instruments
    # ------------------------

    def get_instruments(self):
        if os.path.exists(self.config['INSTRUMENTS_DIR']):
            for file_ in os.listdir(self.config['INSTRUMENTS_DIR']):
                if self.is_valid_instrument_file(file_):
                    self.append_instrument_to_list(file_)

    def append_instrument_to_list(self, zip_filename):
        name_, sha_ = self._tokenize_zipfilename(zip_filename)
        for name, shas in self.instruments.iteritems():
            if name == name_ and (sha_ not in shas):
                shas.append(sha_)
                return
        self.instruments[name_] = [sha_] # New instrument

    def remove_instrument_from_list(self, zip_filename):
        name_, sha_ = self._tokenize_zipfilename(zip_filename)
        for name, shas in self.instruments.iteritems():
            if name == name_ and sha_ in shas:
                shas.remove(sha_)
                if len(shas) == 0:
                    del self.instruments[name]
                return

    def save_uploaded_instrument(self, zip_filename):
        if not os.path.exists(self.config['INSTRUMENTS_DIR']):
            os.makedirs(self.config['INSTRUMENTS_DIR'])
        shutil.copy(zip_filename, self.config['INSTRUMENTS_DIR'])

    def delete_uploaded_instrument(self, zip_filename):
        if os.path.exists(self.config['INSTRUMENTS_DIR']):
            os.remove(os.path.join(self.config['INSTRUMENTS_DIR'], zip_filename))

    def install_instrument(self, zip_filename):
        if not os.path.exists(zip_filename):
            log('error', 'Instrument zip file not found.\nNo installation done.')
            return

        name, sha = self._tokenize_zipfilename(zip_filename)
        print('Installing instrument ' + name + ' with version ' + sha)
        self.stop_client()
        # http://stackoverflow.com/questions/21936597/blocking-and-non-blocking-subprocess-calls
        subprocess.call(['/bin/bash', 'api_app/install_instrument.sh', zip_filename, name])
        self.start_client()

        if not self.client.is_connected:
            self.handle_instrument_install_failure()
            return 'client_not_connected'

        self.live_instrument = {'name': name,
                                'sha': sha,
                                'server_version': self.common.get_server_version()}
                                
        self.store_last_deployed_zip(zip_filename)
        return 'success' if self.is_bitstream_id_valid() else 'invalid_bitstream_id'

    def handle_instrument_install_failure(self):
        # Check whether we are installing the last deployed instrument to avoid infinite recursion:
        last_deployed_instrument = self.get_last_deployed_instrument()
        if ((not 'name' in last_deployed_instrument)
            or (last_deployed_instrument['name'] == self.live_instrument['name']
                and last_deployed_instrument['sha'] == self.live_instrument['sha'])):
                self._start_first_instrument_found(exclude=self.live_instrument)
        else:
            self.start_last_deployed_instrument()

    def is_bitstream_id_valid(self):
        id_ = self.common.get_bitstream_id()
        hash_ = hashlib.sha256(self.live_instrument["name"] + '-' + self.live_instrument["sha"])
        if not hash_.hexdigest() == id_:
            log('error', 'Corrupted instrument: ID mismatch' 
                  + '\n* Bitstream ID:\n' + id_ 
                  + '\n* Expected:\n' + hash_.hexdigest())
            return False
        return True

    def store_last_deployed_zip(self, zip_filename):
        zip_store_filename = os.path.join(self.config['INSTRUMENTS_DIR'], os.path.basename(zip_filename))
        if not os.path.exists(zip_store_filename):
            shutil.copy(zip_filename, self.config['INSTRUMENTS_DIR'])
        with open(os.path.join(self.config['INSTRUMENTS_DIR'], '.instruments'), 'w') as f:
            f.write('last_deployed: ' + zip_store_filename)

    def get_last_deployed_instrument(self):
        with open(os.path.join(self.config['INSTRUMENTS_DIR'], '.instruments'), 'r') as f:
            tokens = f.readline().split(':')
            if tokens[0].strip() != 'last_deployed':
                log('error', 'Corrupted file .instruments')
                return {}
            name, sha = self._tokenize_zipfilename(os.path.basename(tokens[1].strip()))
            return {'name': name, 'sha': sha}
        return {}

    def start_last_deployed_instrument(self):
        log('notice', 'Start last deployed instrument')
        instrument_path = os.path.join(self.config['INSTRUMENTS_DIR'], '.instruments')
        if os.path.exists(instrument_path):
            with open(instrument_path, 'r') as f:
                tokens = f.readline().split(':')
                if tokens[0].strip() != 'last_deployed':
                    log('error', 'Corrupted file .instruments')
                    self._start_first_instrument_found()
                    return
                zip_filename = os.path.join(self.config['INSTRUMENTS_DIR'], 
                                            os.path.basename(tokens[1].strip()))
                if os.path.exists(zip_filename):
                    self.install_instrument(zip_filename)
                else:
                    log('error', 'Last deployed instrument zip file not found')
                    self._start_first_instrument_found()
        else:
            self._start_first_instrument_found()

    def _start_first_instrument_found(self, exclude=None):
        for filename in os.listdir(self.config['INSTRUMENTS_DIR']):
            if self.is_valid_instrument_file(filename):
                name, sha = self._tokenize_zipfilename(filename)
                if exclude == None or (name != exclude['name'] and sha != exclude['sha']):
                    self.install_instrument(
                            os.path.join(self.config['INSTRUMENTS_DIR'], filename))
                    return
                
        log('error', 'No instrument found: Load backup')
        self.start_from_backup()

    def start_from_backup(self):
        backup_dir = self.restore_backup()
        for filename in os.listdir(backup_dir):
            if self.is_valid_instrument_file(filename):
                self.install_instrument(os.path.join(backup_dir, filename))
                return
        log('critical', 'No instrument found')

    def restore_backup(self):
        backup_dir = os.path.join(self.config['INSTRUMENTS_DIR'], 'backup')
        copy_tree(backup_dir, self.config['INSTRUMENTS_DIR'])
        return backup_dir

    def is_valid_instrument_file(self, filename):
        return self.is_zip_file(filename)

    def is_valid_app_file(self, filename):
        return self.is_zip_file(filename)

    def is_zip_file(self, filename):
        filebase = os.path.basename(filename)
        return '.' in filebase and filebase.rsplit('.', 1)[1] == 'zip'
        
    def _tokenize_zipfilename(self, zip_filename):
        tokens = os.path.basename(zip_filename).split('.')[0].split('-')
        name = '-'.join(tokens[:-1])
        sha = tokens[-1]
        return name, sha

api_app = KoheronAPIApp(__name__)

from api_app import api

if __name__ == "__main__":
    app.run(host='0.0.0.0')
