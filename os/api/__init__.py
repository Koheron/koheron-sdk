import os
import json
import subprocess
from flask import Flask, jsonify, request, make_response
from werkzeug.utils import secure_filename
import uwsgi

def is_zip(filename):
    base = os.path.basename(filename)
    return os.path.splitext(base)[1] == '.zip'

def get_name_from_zipfilename(zip_filename):
    base = os.path.basename(zip_filename)
    split = os.path.splitext(base)
    return split[0] if split[1] == '.zip' else None

def is_file_in_zip(zip_filename, target_filename):

    """
    zip_filename = "/usr/local/instruments/led-blinker.zip"
    target_filename = "version" # file in zip_filename
    """

    zip_files_stdout = subprocess.Popen(["/usr/bin/unzip", "-l", zip_filename], stdout=subprocess.PIPE)
    zip_files = zip_files_stdout.stdout.read()

    if target_filename.encode() in zip_files:
        return True
    else:
        return False

def read_file_in_zip(zip_filename, target_filename):

    """
    zip_filename = "/usr/local/instruments/led-blinker.zip"
    target_filename = "version" # file to read in zip_filename
    """

    target_stdout = subprocess.Popen(["/usr/bin/unzip", "-c", zip_filename, target_filename], stdout=subprocess.PIPE)
    target_file_content = target_stdout.stdout.read().splitlines()[2]

    return target_file_content

class KoheronApp(Flask):

    instruments_dirname = "/usr/local/instruments/"
    live_instrument_dirname = "/tmp/live-instrument/"
    default_filename = "default"
    version_filename = "version"

    def __init__(self, *args, **kwargs):
        super(KoheronApp, self).__init__(*args, **kwargs)
        self.init_instruments(KoheronApp.instruments_dirname)

    def get_instrument_dict(self, instrument_filename, is_default, version_filename):

        instrument = {}
        instrument["name"] = get_name_from_zipfilename(instrument_filename)

        version = "0.0.0"

        if (is_file_in_zip(instrument_filename, version_filename)):
            version = read_file_in_zip(instrument_filename, version_filename)

        instrument["version"] = version
        instrument["is_default"] = is_default

        return instrument

    def is_default_instrument(self, instrument_filename, instruments_dirname, default_filename):

        with open(os.path.join(instruments_dirname, default_filename), 'r') as f:
            default_instrument_filename = os.path.join(instruments_dirname, f.read().rstrip('\n'))

        if instrument_filename == default_instrument_filename:
            return True
        else:
            return False

    def init_instruments(self, instruments_dirname):

        self.instruments_list = []

        for filename in (x for x in os.listdir(instruments_dirname) if x.endswith(".zip"))::

            instrument_filename = os.path.join(instruments_dirname, filename)
            is_default = self.is_default_instrument(instrument_filename, instruments_dirname, KoheronApp.default_filename)

            instrument = self.get_instrument_dict(instrument_filename, is_default, KoheronApp.version_filename)

            if instrument["name"] is not None:

                self.instruments_list.append(instrument)

            if is_default:

                self.run_instrument(instrument_filename, KoheronApp.live_instrument_dirname, instrument)

    def run_instrument(self, instrument_filename, live_instrument_dirname, instrument_dict):

        if not os.path.exists(instrument_filename):
            print('Instrument zip file not found.\nNo installation done.')
            return
        name = get_name_from_zipfilename(instrument_filename)
        print('Installing instrument ' + name)
        subprocess.call(['/bin/bash', 'app/install_instrument.sh', name, live_instrument_dirname])

        self.live_instrument = instrument_dict

        return 'success'

app = KoheronApp(__name__)

# ------------------------
# Instruments
# ------------------------

@app.route('/api/instruments', methods=['GET'])
def get_instruments_status():
    instruments_status_list = []
    for instrument in app.instruments_list:
        instruments_status_list.append(instrument['name'])
    instrument_status_live = app.live_instrument['name']
    return jsonify({'instruments': instruments_status_list, 'live_instrument': instrument_status_live })

@app.route('/api/instruments/details', methods=['GET'])
def get_instruments_details():
    return jsonify({'instruments': app.instruments_list, 'live_instrument': app.live_instrument })

@app.route('/api/instruments/run/<name>', methods=['GET'])
def run_instrument(name):
    zip_filename = '{}.zip'.format(name)
    filename = os.path.join(app.instruments_dirname, secure_filename(zip_filename))
    is_default = app.is_default_instrument(os.path.join(app.instruments_dirname, secure_filename(filename)), app.instruments_dirname, app.default_filename)
    instrument_dict = app.get_instrument_dict(filename, is_default, app.version_filename)
    status = app.run_instrument(filename, app.live_instrument_dirname, instrument_dict)
    if status == 'success':
        response = 'Instrument %s successfully installed' % zip_filename
    else:
        response = 'Failed to install instrument %s' % zip_filename
    return make_response(response)

@app.route('/api/instruments/delete/<name>', methods=['GET'])
def delete_instrument(name):
    zip_filename = secure_filename('{}.zip'.format(name))

    instrument_filename = os.path.join(app.instruments_dirname, zip_filename)
    if os.path.exists(instrument_filename):
        os.remove(instrument_filename)

    for instrument in app.instruments_list:

        if instrument["name"] == name:

            if instrument["is_default"]:

                return make_response('Default instrument cannot be removed')

            else:

                app.instruments_list.remove(instrument)
                return make_response('Instrument ' + zip_filename + ' removed.')

@app.route('/api/instruments/upload', methods=['POST'])
def upload_instrument():
    if request.method == 'POST':
        filename = next((filename for filename in request.files if is_zip(filename)), None)
        if filename is not None:
            request.files[filename].save(os.path.join(app.instruments_dirname, secure_filename(filename)))

            is_default = app.is_default_instrument(os.path.join(app.instruments_dirname, secure_filename(filename)), app.instruments_dirname, app.default_filename)
            instrument = app.get_instrument_dict(os.path.join(app.instruments_dirname, secure_filename(filename)), is_default, app.version_filename)

            is_instrument_in_list = False

            for listed_instrument in app.instruments_list:

                if listed_instrument["name"] == instrument["name"]:

                    if listed_instrument["version"] != instrument["name"]:

                        app.instruments_list.remove(listed_instrument)

                    else:

                        is_instrument_in_list = True

            if not (is_instrument_in_list):

                app.instruments_list.append(instrument)

            return make_response('Instrument ' + filename + ' uploaded.')
    return make_response('Instrument upload failed.')