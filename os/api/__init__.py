import os
import subprocess
import zipfile
from datetime import datetime, timezone

from flask import Flask, jsonify, request, make_response, Response
from werkzeug.utils import secure_filename

from systemd import journal as _sd_journal


INVOCATION_ID = os.environ.get("INVOCATION_ID")


DEFAULT_UNIT = "koheron-server.service"
MAX_LINES = 5000


def _ts_us(entry) -> int | None:
    value = entry.get("__REALTIME_TIMESTAMP")
    if isinstance(value, (int, float)):
        return int(value)
    if hasattr(value, "timestamp"):
        return int(value.timestamp() * 1_000_000)
    if isinstance(value, str):
        try:
            return int(value)
        except ValueError:
            return None
    return None


def _now_ts_us() -> int:
    return int(datetime.now(timezone.utc).timestamp() * 1_000_000)


def _combine_ts(*values: int | None) -> int | None:
    filtered = [v for v in values if isinstance(v, (int, float)) and v > 0]
    return int(max(filtered)) if filtered else None


def _reader(*, invocation_id: str | None = None) -> "_sd_journal.Reader":
    reader = _sd_journal.Reader()
    try:
        reader.this_boot()
    except Exception:
        pass
    reader.add_match(_SYSTEMD_UNIT=DEFAULT_UNIT)
    if invocation_id:
        try:
            reader.add_match(_SYSTEMD_INVOCATION_ID=invocation_id)
        except Exception:
            pass
    return reader


def _detect_invocation_id() -> str | None:
    if INVOCATION_ID:
        return INVOCATION_ID
    try:
        reader = _reader()
        reader.seek_tail()
        entry = reader.get_previous()
        if entry:
            return entry.get("_SYSTEMD_INVOCATION_ID")
    except Exception:
        pass
    return None


def _collect_from_tail(reader: "_sd_journal.Reader", limit: int) -> list:
    if limit <= 0:
        limit = MAX_LINES
    collected = []
    try:
        reader.seek_tail()
    except Exception:
        return collected
    count = 0
    while count < limit:
        entry = reader.get_previous()
        if not entry:
            break
        collected.append(entry)
        count += 1
    collected.reverse()
    return collected


def _collect_from_cursor(reader: "_sd_journal.Reader", cursor: str, limit: int) -> list:
    if limit <= 0 or limit > MAX_LINES:
        limit = MAX_LINES
    try:
        reader.seek_cursor(cursor)
        reader.get_next()  # move past the cursor entry
    except Exception:
        return []
    collected = []
    count = 0
    while count < limit:
        entry = reader.get_next()
        if not entry:
            break
        collected.append(entry)
        count += 1
    return collected


def _normalize_entries(entries: list, *, min_ts: int | None) -> tuple[list, str | None]:
    normalized: list[dict] = []
    last_cursor: str | None = None
    for entry in entries:
        ts = _ts_us(entry)
        last_cursor = entry.get("__CURSOR", last_cursor)
        if min_ts is not None and ts is not None and ts < min_ts:
            continue
        normalized.append({"ts": ts, "msg": entry.get("MESSAGE", "")})
    if min_ts is not None and normalized:
        # drop any entries that barely made it due to cursor alignment
        normalized = [e for e in normalized if e.get("ts") is None or e["ts"] >= min_ts]
    return normalized, last_cursor


def _read(
    cursor: str | None,
    n: int,
    *,
    min_ts: int | None = None,
    invocation_id: str | None = None,
    allow_fallback: bool = True,
) -> dict:
    reader = _reader(invocation_id=invocation_id)
    limit = MAX_LINES if n <= 0 else min(n, MAX_LINES)

    if cursor:
        raw_entries = _collect_from_cursor(reader, cursor, limit)
        if not raw_entries:
            return {"cursor": None, "entries": []}
    else:
        raw_entries = _collect_from_tail(reader, limit)

    entries, last_cursor = _normalize_entries(raw_entries, min_ts=min_ts)

    if not entries and last_cursor is None and invocation_id and allow_fallback:
        return _read(
            cursor=cursor,
            n=n,
            min_ts=min_ts,
            invocation_id=None,
            allow_fallback=False,
        )

    if not cursor and limit > 0 and len(entries) > limit:
        entries = entries[-limit:]

    return {"cursor": last_cursor, "entries": entries}


def _bookmark_tail(
    *, invocation_id: str | None = None, allow_fallback: bool = True
) -> tuple[str | None, int | None, str | None]:
    reader = _reader(invocation_id=invocation_id)
    try:
        reader.seek_tail()
        entry = reader.get_previous()
    except Exception:
        entry = None

    if entry:
        return (
            entry.get("__CURSOR"),
            _ts_us(entry),
            entry.get("_SYSTEMD_INVOCATION_ID"),
        )

    if invocation_id and allow_fallback:
        return _bookmark_tail(invocation_id=None, allow_fallback=False)

    return None, None, None

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
        self.instrument_log_cursor: str | None = None
        self.instrument_log_ts: int | None = None
        initial_invocation = _detect_invocation_id()
        cursor, ts, tail_invocation = _bookmark_tail(invocation_id=initial_invocation)
        self.instrument_invocation_id: str | None = tail_invocation or initial_invocation
        self.instrument_log_cursor = cursor
        self.instrument_log_ts = _combine_ts(ts)

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
        start_cursor, start_ts, start_invocation = _bookmark_tail(
            invocation_id=self.instrument_invocation_id
        )
        if start_invocation:
            self.instrument_invocation_id = start_invocation
        if start_ts is None:
            start_ts = _now_ts_us()
        result = subprocess.call(['/bin/bash', 'app/install_instrument.sh', name, live_instrument_dirname])

        if result == 0:
            self.live_instrument = instrument_dict
            post_cursor, post_ts, post_invocation = _bookmark_tail()
            if post_invocation:
                self.instrument_invocation_id = post_invocation
            baseline_cursor = start_cursor or post_cursor
            baseline_ts = start_ts if start_cursor else post_ts
            if baseline_ts is None:
                baseline_ts = _now_ts_us()
            self.instrument_log_cursor = baseline_cursor
            self.instrument_log_ts = _combine_ts(baseline_ts)
            return 'success'
        return 'failed'

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

@app.route('/api/instruments/commands/<name>', methods=['GET'])
def download_instrument_commands(name: str):
    zip_filename = secure_filename(f"{name}.zip")
    instrument_filename = os.path.join(app.instruments_dirname, zip_filename)

    if not os.path.exists(instrument_filename):
        return make_response('Instrument not found', 404)

    try:
        with zipfile.ZipFile(instrument_filename) as zf:
            try:
                with zf.open('drivers.json') as drivers_file:
                    data = drivers_file.read()
            except KeyError:
                return make_response('drivers.json not found', 404)
    except zipfile.BadZipFile:
        return make_response('Invalid instrument archive', 400)

    response = Response(data, mimetype='application/json')
    response.headers['Content-Disposition'] = f'attachment; filename={name}-drivers.json'
    return response

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


@app.route("/api/logs/koheron/bookmark", methods=["GET"])
def logs_bookmark():
    """Return the latest journal cursor and timestamp without streaming entries."""

    data = _read(cursor=None, n=1)
    entries = data.get("entries", [])
    ts = entries[-1]["ts"] if entries else None
    return jsonify({"cursor": data.get("cursor"), "ts": ts})


@app.route("/api/logs/koheron/instrument/incr", methods=["GET"])
def logs_instrument_incr():
    if app.instrument_invocation_id is None:
        app.instrument_invocation_id = _detect_invocation_id()
    requested_cursor = request.args.get("cursor")
    cursor = requested_cursor or app.instrument_log_cursor
    min_ts = app.instrument_log_ts if cursor is None else None
    data = _read(
        cursor=cursor,
        n=0,
        min_ts=min_ts,
        invocation_id=app.instrument_invocation_id,
    )
    if data.get("cursor"):
        app.instrument_log_cursor = data["cursor"]
    entries = data.get("entries", [])
    if entries:
        app.instrument_log_ts = _combine_ts(app.instrument_log_ts, entries[-1].get("ts"))
    return jsonify(data)


@app.route("/api/logs/koheron/instrument/bookmark", methods=["GET"])
def logs_instrument_bookmark():
    if app.instrument_invocation_id is None:
        app.instrument_invocation_id = _detect_invocation_id()
    cursor = app.instrument_log_cursor
    ts = app.instrument_log_ts
    if cursor is None and ts is None:
        cursor, ts, inv = _bookmark_tail(invocation_id=app.instrument_invocation_id)
        if inv:
            app.instrument_invocation_id = inv
        app.instrument_log_cursor = cursor
        app.instrument_log_ts = _combine_ts(ts, _now_ts_us())
        cursor = app.instrument_log_cursor
        ts = app.instrument_log_ts
    return jsonify({"cursor": cursor, "ts": ts})

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
