import os
import json
import subprocess
import zipfile
from flask import Flask, jsonify, request, make_response, Response
from werkzeug.utils import secure_filename

from systemd import journal as _sd_journal
from datetime import datetime, timezone


DEFAULT_UNIT = "koheron-server.service"
MAX_LINES = 5000

def _ts_us(e) -> int:
    v = e.get("__REALTIME_TIMESTAMP", 0)
    if isinstance(v, (int, float)): return int(v)
    if hasattr(v, "timestamp"):      return int(v.timestamp() * 1_000_000)
    if isinstance(v, str):
        try: return int(v)
        except: return 0
    return 0

def _reader() -> "_sd_journal.Reader":
    r = _sd_journal.Reader()
    try: r.this_boot()         # keep results to current boot
    except Exception: pass
    r.add_match(_SYSTEMD_UNIT=DEFAULT_UNIT)
    return r

def _read(cursor: str | None, n: int) -> dict:
    r = _reader()
    limit = MAX_LINES if n <= 0 else min(n, MAX_LINES)
    entries, last = [], None

    if cursor:
        try:
            r.seek_cursor(cursor); r.get_next()   # skip the cursor entry
        except Exception:
            return {"cursor": None, "entries": []}
    else:
        r.seek_tail()
        for _ in range(limit):
            if not r.get_previous(): break

    for e in r:
        entries.append({"ts": _ts_us(e), "msg": e.get("MESSAGE", "")})
        last = e.get("__CURSOR", last)
        if cursor and len(entries) >= limit:
            break
    return {"cursor": last, "entries": entries}

def is_zip(filename):
    base = os.path.basename(filename)
    return os.path.splitext(base)[1] == '.zip'

def get_name_from_zipfilename(zip_filename):
    base = os.path.basename(zip_filename)
    split = os.path.splitext(base)
    return split[0] if split[1] == '.zip' else None

def zip_has_file(zip_filename: str, member: str) -> bool:
    try:
        with zipfile.ZipFile(zip_filename) as zf:
            zf.getinfo(member)  # raises KeyError if missing
            return True
    except (KeyError, zipfile.BadZipFile, FileNotFoundError):
        return False

def read_text_from_zip(zip_filename: str, member: str) -> str | None:
    try:
        with zipfile.ZipFile(zip_filename) as zf, zf.open(member) as f:
            return f.read().decode("utf-8", errors="replace").strip()
    except Exception:
        return None

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

        version = read_text_from_zip(instrument_filename, version_filename) or "0.0.0"

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

        for filename in (x for x in os.listdir(instruments_dirname) if x.endswith(".zip")):
            instrument_filename = os.path.join(instruments_dirname, filename)
            is_default = self.is_default_instrument(instrument_filename, instruments_dirname, KoheronApp.default_filename)

            instrument = self.get_instrument_dict(instrument_filename, is_default, KoheronApp.version_filename)

            if instrument["name"] is not None:
                self.instruments_list.append(instrument)

            if is_default:
                self.live_instrument = instrument
                #self.run_instrument(instrument_filename, KoheronApp.live_instrument_dirname, instrument)

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
    is_default = app.is_default_instrument(filename, app.instruments_dirname, app.default_filename)
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
                    if listed_instrument["version"] != instrument["version"]:
                        app.instruments_list.remove(listed_instrument)
                    else:
                        is_instrument_in_list = True

            if not (is_instrument_in_list):
                app.instruments_list.append(instrument)

            return make_response('Instrument ' + filename + ' uploaded.')
    return make_response('Instrument upload failed.')

# ------------------------
# Koheron server log
# ------------------------

@app.route("/api/logs/koheron", methods=["GET"])
def logs_tail():
    lines = int(request.args.get("lines", 200))
    data = _read(cursor=None, n=lines)
    if not data["entries"]:
        return jsonify({"error": f"no logs for {DEFAULT_UNIT}"}), 404
    return jsonify(data)

@app.route("/api/logs/koheron/incr", methods=["GET"])
def logs_incr():
    cursor = request.args.get("cursor")
    return jsonify(_read(cursor=cursor, n=0))

# ------------------------
# System manifest / release as JSON
# ------------------------

MANIFEST_PATH = "/usr/local/share/koheron/manifest.txt"
RELEASE_PATH  = "/etc/koheron-release"

def _parse_kv_file(path: str) -> dict | None:
    """Parse simple key=value files. Returns dict or None if file missing."""
    if not os.path.exists(path):
        return None
    data: dict[str, str] = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                k, v = line.split("=", 1)
                data[k.strip()] = v.strip()
    return data

@app.route("/api/system/manifest", methods=["GET"])
def api_system_manifest():
    data = _parse_kv_file(MANIFEST_PATH)
    if data is None:
        return jsonify({"error": "manifest not found"}), 404
    return jsonify(data)

@app.route("/api/system/release", methods=["GET"])
def api_system_release():
    data = _parse_kv_file(RELEASE_PATH)
    if data is None:
        return jsonify({"error": "release file not found"}), 404
    return jsonify(data)

@app.route("/api/system/build", methods=["GET"])
def api_system_build():
    """Combined view for convenience."""
    manifest = _parse_kv_file(MANIFEST_PATH) or {}
    release  = _parse_kv_file(RELEASE_PATH)  or {}
    return jsonify({"manifest": manifest, "release": release})

# Raw files downloads
@app.route("/api/system/manifest/raw", methods=["GET"])
def api_system_manifest_raw():
    if not os.path.exists(MANIFEST_PATH):
        return make_response("manifest not found", 404)
    with open(MANIFEST_PATH, "rb") as f:
        return Response(f.read(), mimetype="text/plain")

@app.route("/api/system/release/raw", methods=["GET"])
def api_system_release_raw():
    if not os.path.exists(RELEASE_PATH):
        return make_response("release not found", 404)
    with open(RELEASE_PATH, "rb") as f:
        return Response(f.read(), mimetype="text/plain")
