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

@api_app.route('/api/version', methods=['GET'])
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

# ------------------------
# Laser 
# ------------------------
# These routes should be optional depending on the configuration

# @api_app.route('/api/laser/current/<current>', methods=['GET'])
# def set_laser_current(current):
#     api_app.laser.set_laser_current(float(current))
#     return make_response('Laser current set to {} mA'.format(current))

# @api_app.route('/api/laser/save', methods=['GET'])
# def save_laser_config():
#     api_app.laser.save_config()
#     return make_response('Laser configuration saved')

# @api_app.route('/api/laser/load', methods=['GET'])
# def load_laser_config():
#     api_app.laser.load_config()
#     return make_response('Laser configuration loaded')

# @api_app.route('/api/laser/stop', methods=['GET'])
# def stop_laser():
#     api_app.laser.stop_laser()
#     return make_response('Laser stopped')

# @api_app.route('/api/laser/start', methods=['GET'])
# def start_laser():
#     api_app.laser.start_laser()
#     return make_response('Laser started')

# @api_app.route('/api/laser/status', methods=['GET'])
# def get_laser_status():
#     (laser_on, current, power) = api_app.laser.get_status()
#     return jsonify({'laser_on': laser_on, 'current': current, 'power': power})

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
    return make_response('Instrument ' + zip_filename + ' installed with status: ' + str(status))

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

@api_app.route('/api/instruments/current', methods=['GET'])
def get_current_instrument():
    return jsonify(api_app.current_instrument)

@api_app.route('/api/instruments/restore', methods=['GET'])
def restore_backup_instruments():
    api_app.restore_backup()
    api_app.get_instruments()
    return make_response('Backup instruments restored.')
