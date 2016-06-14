from api_app import api_app

from flask import Flask, jsonify, request, url_for, make_response
from werkzeug import secure_filename

import urllib
import os
import subprocess
import uwsgi

api_app.config['UPLOAD_FOLDER'] = '/tmp'

# ------------------------
# API
# ------------------------

@api_app.route('/api/upload_latest_app', methods=['GET'])
def upload_latest_app():
    if api_app.upload_latest_app() < 0:
        return make_response('failure')
    else:
        return make_response('success')

@api_app.route('/api/upgrade_app', methods=['GET'])
def upgrade_app():
    if api_app.upload_latest_app() < 0:
        return make_response('failure')
    else:
       api_app.unzip_app()
       api_app.copy_ui_to_static()
       uwsgi.reload()
       return make_response('Upgrading')

@api_app.route('/api/infos', methods=['GET'])
def api_version():
    return jsonify(api_app.metadata)

@api_app.route('/api/remote_apps', methods=['GET'])
def remote_apps():
    return jsonify({'apps': api_app.remote_apps})

@api_app.route('/api/reboot', methods=['GET'])
def reboot():
    subprocess.call(['/sbin/reboot'])
    return make_response('Rebooting system ...')

@api_app.route('/api/ping', methods=['GET','POST'])
def ping():
    api_app.ping()
    return make_response("Done !!")

@api_app.route('/api/bitstream_id', methods=['GET'])
def bitstream_id():
    return make_response(api_app.get_bitstream_id())

@api_app.route('/api/upload/remote/<zip_filename>', methods=['GET', 'POST'])
def upload_remote_instrument(zip_filename):
    filename = secure_filename(zip_filename)
    tmp_file = os.path.join('/tmp/', filename)
    urllib.urlretrieve(app.config['S3_URL'] + filename, tmp_file)
    api_app.append_local_instrument(tmp_file)
    api_app.save_uploaded_instrument(tmp_file)
    return make_response('Instrument ' + filename + ' uploaded.')

@api_app.route('/api/deploy/remote/<zip_filename>', methods=['GET', 'POST'])
def deploy_remote_instrument(zip_filename):
    filename = secure_filename(zip_filename)
    urllib.urlretrieve(api_app.config['S3_URL'] + filename, '/tmp/' + filename)
    status = api_app.install_instrument('/tmp/' + filename)
    return make_response('Instrument ' + zip_filename + ' installed with status: ' + str(status))

@api_app.route('/api/deploy/local/<zip_filename>', methods=['GET'])
def deploy_local_instrument(zip_filename):
    filename = os.path.join(api_app.config['INSTRUMENTS_DIR'], secure_filename(zip_filename))
    status = api_app.install_instrument(filename)
    return make_response('Instrument ' + zip_filename + ' installed with status: ' + str(status))

@api_app.route('/api/remove/local/<zip_filename>', methods=['GET'])
def remove_local_instrument(zip_filename):
    filename = secure_filename(zip_filename)
    api_app.delete_uploaded_instrument(filename)
    api_app.remove_local_instrument(filename)
    return make_response('File ' + zip_filename + ' removed.')

@api_app.route('/api/upload/instrument_zip', methods=['GET', 'POST'])
def upload_instrument_zip():
    if request.method == 'POST':
        file_ = next((file_ for file_ in request.files if api_app.is_valid_instrument_file(file_)), None)
        if file_ is not None:
            filename = secure_filename(file_)
            request.files[file_].save(os.path.join(api_app.config['UPLOAD_FOLDER'], filename))
            tmp_file = os.path.join('/tmp/', filename)
            api_app.append_local_instrument(tmp_file)
            api_app.save_uploaded_instrument(tmp_file)
            return make_response('Instrument ' + filename + ' uploaded.')
    return make_response('Instrument upload failed.')

@api_app.route('/api/get_local_instruments', methods=['GET'])
def get_local_instruments():
    return jsonify(api_app.local_instruments)

@api_app.route('/api/get_current_instrument', methods=['GET'])
def get_current_instrument():
    return jsonify(api_app.current_instrument)
