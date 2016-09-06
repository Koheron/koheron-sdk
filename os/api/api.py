from api_app import api_app

from flask import Flask, jsonify, request, url_for, make_response
from werkzeug import secure_filename

import urllib
import os
import subprocess
import uwsgi

api_app.config['UPLOAD_FOLDER'] = '/tmp'

# ------------------------
# HTTP API base functions
# ------------------------

@api_app.route('/api/app/update', methods=['POST'])
def upgrade_app():
    if request.method == 'POST':
        file_ = next((file_ for file_ in request.files if api_app.is_valid_app_file(file_)), None)
        if file_ is not None:
            filename = secure_filename(file_)
            request.files[file_].save(os.path.join(api_app.config['UPLOAD_FOLDER'], filename))
            tmp_file = os.path.join('/tmp/', filename)
            subprocess.call(['/usr/bin/unzip', '-o', tmp_file, '-d', '/usr/local/flask'])
            uwsgi.reload()
            return make_response('Updating app')

@api_app.route('/api/app/version', methods=['GET'])
def api_version():
    return jsonify(api_app.metadata)

@api_app.route('/api/app/remote', methods=['GET'])
def remote_apps():
    return jsonify({'apps': api_app.remote_apps})

# ------------------------
# Static
# ------------------------

@api_app.route('/api/static/update', methods=['GET'])
def update_static():
    if api_app.upload_latest_static() < 0:
        return make_response('Upload failed')
    else:
       api_app.unzip_static()
       api_app.copy_static()
       return make_response('Updating app')

@api_app.route('/api/static/upload', methods=['POST'])
def upload_static():
    if request.method == 'POST':
        file_ = next((file_ for file_ in request.files if api_app.is_zip_file(file_)), None)
        if file_ is not None:
            request.files[file_].save('/tmp/static.zip')
            api_app.unzip_static()
            api_app.copy_static()
            return make_response('Static upload success')
    return make_response('Static upload failed.')

# ------------------------
# Board
# ------------------------

@api_app.route('/api/board/reboot', methods=['GET'])
def reboot():
    subprocess.call(['/sbin/reboot'])
    return make_response('Rebooting system ...')

@api_app.route('/api/board/version', methods=['GET'])
def version():
    with open('/etc/zynq_sdk_version','r') as f:
        zynq_sdk_version = f.read()
    return jsonify({'zynq-sdk': zynq_sdk_version})

@api_app.route('/api/board/dna', methods=['GET'])
def dna():
    return make_response(api_app.common.get_dna())

@api_app.route('/api/board/bitstream_id', methods=['GET'])
def bitstream_id():
    return make_response(api_app.common.get_bitstream_id())

@api_app.route('/api/board/ping', methods=['GET'])
def ping():
    api_app.ping()
    return make_response("Done !!")

@api_app.route('/api/board/init', methods=['GET'])
def init():
    api_app.init()
    return make_response("Initialisation done")

# ------------------------
# Instruments
# ------------------------

@api_app.route('/api/instruments/update', methods=['GET'])
def update_instruments():
    return make_response("update instrument not implemented")

@api_app.route('/api/instruments/run/<name>/<sha>', methods=['GET'])
def run_instrument(name, sha):
    zip_filename = '{}-{}.zip'.format(name, sha)
    filename = os.path.join(api_app.config['INSTRUMENTS_DIR'], secure_filename(zip_filename))
    status = api_app.install_instrument(filename)
    if status == 'success':
        response = 'Instrument %s successfully installed' % zip_filename
    elif status == 'invalid_bitstream':
        response = 'Instrument %s installed (warning: invalid bitstream ID)' % zip_filename
    else:
        response = 'Failed to install instrument %s' % zip_filename
    return make_response(response)

@api_app.route('/api/instruments/delete/<name>/<sha>', methods=['GET'])
def delete_instrument(name, sha):
    zip_filename = '{}-{}.zip'.format(name, sha)
    filename = secure_filename(zip_filename)
    api_app.delete_uploaded_instrument(filename)
    api_app.remove_instrument_from_list(filename)
    return make_response('File ' + zip_filename + ' removed.')

@api_app.route('/api/instruments/upload', methods=['POST'])
def upload_instrument():
    if request.method == 'POST':
        file_ = next((file_ for file_ in request.files if api_app.is_valid_instrument_file(file_)), None)
        if file_ is not None:
            filename = secure_filename(file_)
            request.files[file_].save(os.path.join(api_app.config['UPLOAD_FOLDER'], filename))
            tmp_file = os.path.join('/tmp/', filename)
            api_app.append_instrument_to_list(tmp_file)
            api_app.save_uploaded_instrument(tmp_file)
            return make_response('Instrument ' + filename + ' uploaded.')
    return make_response('Instrument upload failed.')

@api_app.route('/api/instruments/upload/<name>/<sha>', methods=['GET'])
def upload_remote_instrument(name, sha):
    filename = secure_filename(zip_filename)
    tmp_file = os.path.join('/tmp/', filename)
    urllib.urlretrieve(app.config['S3_URL'] + filename, tmp_file)
    api_app.append_instrument_to_list(tmp_file)
    api_app.save_uploaded_instrument(tmp_file)
    return make_response('Instrument ' + filename + ' uploaded.')

@api_app.route('/api/instruments/local', methods=['GET'])
def get_local_instruments():
    return jsonify(api_app.local_instruments)

@api_app.route('/api/instruments/live', methods=['GET'])
def get_live_instrument():
    return jsonify(api_app.live_instrument)

@api_app.route('/api/instruments/restore', methods=['GET'])
def restore_backup_instruments():
    api_app.restore_backup()
    api_app.get_instruments()
    return make_response('Backup instruments restored.')
