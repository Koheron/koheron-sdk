import os
import json
import subprocess
from flask import Flask, jsonify, request, make_response
from werkzeug import secure_filename
import uwsgi

def is_valid_instrument_file(filename):
    base = os.path.basename(filename)
    return os.path.splitext(base)[1] == '.zip'

def get_name_from_zipfilename(zip_filename):
    base = os.path.basename(zip_filename)
    split = os.path.splitext(base)
    return split[0] if split[1] == '.zip' else None

class KoheronApp(Flask):
    def __init__(self, *args, **kwargs):
        super(KoheronApp, self).__init__(*args, **kwargs)
        self.init_instruments_list()
        self.init_live_instrument()

    def init_instruments_list(self):
        self.instruments_list = []
        for filename in os.listdir("/usr/local/instruments/"):

            name = get_name_from_zipfilename(filename)

            if name is not None:

                instrument_files_stdout = subprocess.Popen(["/usr/bin/unzip", "-l", "/usr/local/instruments/" + filename], stdout=subprocess.PIPE)
                instrument_files = instrument_files_stdout.stdout.read()

                instrument = {}
                instrument['name'] = name

                version = ""

                if 'version' in instrument_files:
                    version_stdout = subprocess.Popen(["/usr/bin/unzip", "-c", "/usr/local/instruments/" + filename, "version"], stdout=subprocess.PIPE)
                    version = version_stdout.stdout.read().splitlines()[2]
                else:
                    version = "0.0.0"

                instrument['version'] = version

                self.instruments_list.append(instrument)


    def init_live_instrument(self):
        # Run last started instrument
        with open('/usr/local/instruments/default', 'r') as f:
            default_inst_filename = os.path.join('/usr/local/instruments/', f.read().rstrip('\n'))
            self.run_instrument(default_inst_filename)

    def run_instrument(self, zip_filename):

        if not os.path.exists(zip_filename):
            print('Instrument zip file not found.\nNo installation done.')
            return
        name = get_name_from_zipfilename(zip_filename)
        print('Installing instrument ' + name)
        live_instrument_dirname = "/tmp/live-instrument"
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
    filename = os.path.join('/usr/local/instruments/', secure_filename(zip_filename))
    status = app.run_instrument(filename)
    if status == 'success':
        response = 'Instrument %s successfully installed' % zip_filename
    else:
        response = 'Failed to install instrument %s' % zip_filename
    return make_response(response)

@app.route('/api/instruments/delete/<name>', methods=['GET'])
def delete_instrument(name):
    zip_filename = secure_filename('{}.zip'.format(name))

    instrument_filename = os.path.join('/usr/local/instruments/', zip_filename)
    if os.path.exists(instrument_filename):
        os.remove(instrument_filename)

    for instrument in app.instruments_list:
        if instrument["name"] == name:
            app.instruments_list.remove(instrument)

    return make_response('File ' + zip_filename + ' removed.')

@app.route('/api/instruments/upload', methods=['POST'])
def upload_instrument():
    if request.method == 'POST':
        filename = next((filename for filename in request.files if is_valid_instrument_file(filename)), None)
        if filename is not None:
            request.files[filename].save(os.path.join('/usr/local/instruments/', secure_filename(filename)))

            name = get_name_from_zipfilename(filename)

            instrument_files_stdout = subprocess.Popen(["/usr/bin/unzip", "-l", "/usr/local/instruments/" + filename], stdout=subprocess.PIPE)
            instrument_files = instrument_files_stdout.stdout.read()

            instrument = {}
            instrument['name'] = name

            version = ""

            if 'version' in instrument_files:
                version_stdout = subprocess.Popen(["/usr/bin/unzip", "-c", "/usr/local/instruments/" + filename, "version"], stdout=subprocess.PIPE)
                version = version_stdout.stdout.read().splitlines()[2]

            else:
                version = "0.0.0"

            instrument['version'] = version

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