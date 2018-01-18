import os
import json
import subprocess
from flask import Flask, jsonify, request, make_response
from werkzeug import secure_filename
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

    if target_filename in zip_files:
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
    default_instrument_filename = instruments_dirname + "default"

    def __init__(self, *args, **kwargs):
        super(KoheronApp, self).__init__(*args, **kwargs)
        self.init_instruments_list(KoheronApp.instruments_dirname)
        self.init_live_instrument(KoheronApp.instruments_dirname, KoheronApp.live_instrument_dirname, KoheronApp.default_instrument_filename)

    def init_instruments_list(self, instruments_dirname):
        self.instruments_list = []
        for filename in os.listdir(instruments_dirname):

            name = get_name_from_zipfilename(filename)

            if name is not None:

                instrument = {}
                instrument['name'] = name

                version = "" # "0.0.0"
                version_filename = "version"

                if (is_file_in_zip(instruments_dirname + filename, version_filename)):
                    version = read_file_in_zip(instruments_dirname + filename, version_filename)
                else:
                    version = "0.0.0"

                instrument["version"] = version

                self.instruments_list.append(instrument)


    def init_live_instrument(self, instruments_dirname, live_instrument_dirname, default_instrument_filename):
        # Run last started instrument
        with open(default_instrument_filename, 'r') as f:
            default_inst_filename = os.path.join(instruments_dirname, f.read().rstrip('\n'))
            self.run_instrument(default_inst_filename, live_instrument_dirname)

    def run_instrument(self, zip_filename, live_instrument_dirname):

        if not os.path.exists(zip_filename):
            print('Instrument zip file not found.\nNo installation done.')
            return
        name = get_name_from_zipfilename(zip_filename)
        print('Installing instrument ' + name)
        subprocess.call(['/bin/bash', 'app/install_instrument.sh', name, live_instrument_dirname])

        self.live_instrument = {}
        self.live_instrument["name"] = name

        version = ""

        if os.path.isfile(os.path.join(live_instrument_dirname,"version")):
            with open(os.path.join(live_instrument_dirname,"version"), "r") as f:
                version = f.read().rstrip('\n')
        else:
            version = "0.0.0"

        self.live_instrument["version"] = version

        return 'success'

app = KoheronApp(__name__)

# ------------------------
# Instruments
# ------------------------

@app.route('/api/instruments', methods=['GET'])
def get_instruments_status():
    return jsonify({'instruments': app.instruments_list, 'live_instrument': app.live_instrument })

@app.route('/api/instruments/run/<name>', methods=['GET'])
def run_instrument(name):
    zip_filename = '{}.zip'.format(name)
    filename = os.path.join(app.instruments_dirname, secure_filename(zip_filename))
    status = app.run_instrument(filename, app.live_instrument_dirname)
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
            app.instruments_list.remove(instrument)

    return make_response('File ' + zip_filename + ' removed.')

@app.route('/api/instruments/upload', methods=['POST'])
def upload_instrument():
    if request.method == 'POST':
        filename = next((filename for filename in request.files if is_zip(filename)), None)
        if filename is not None:
            request.files[filename].save(os.path.join(app.instruments_dirname, secure_filename(filename)))

            name = get_name_from_zipfilename(filename)

            instrument = {}
            instrument['name'] = name

            version = "" # "0.0.0"
            version_filename = "version"

            if (is_file_in_zip(app.instruments_dirname + filename, version_filename)):
                version = read_file_in_zip(app.instruments_dirname + filename, version_filename)
            else:
                version = "0.0.0"

            instrument["version"] = version

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