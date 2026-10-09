# Red Pitaya nginx validation

Measured on 2026-10-10 at `192.168.1.85`, MAC `00:26:32:f0:1c:6a`, with
Ubuntu Base 26.04.1, nginx 1.28.3, Linux 6.18.0-xilinx and the FFT instrument.
The board reports the `red-pitaya-fft` image release and `xlnx,zynq-7000`
device-tree compatibility. Its image build ID was
`20261008.230342.cab6f8ac9bf4-dirty`.

Raw measurements, response hashes and tested configuration hashes are in
[red-pitaya-2026-10-10.json](red-pitaya-2026-10-10.json).

## Method

Two temporary nginx instances used the baseline configuration from `30361b83`
on port 18080 and the proposed configuration on port 18081. Both served the
board's existing `/tmp/live-instrument` and `/usr/local/www` and connected to
the real uWSGI socket and FFT server. Only listen ports, PID/log/temp paths,
and the site include path were changed for isolation. Extra distribution
`conf.d` files were excluded from both instances. Candidate temporary files
used a private directory under the board's `/tmp` tmpfs instead of `/run`.
Both configurations passed the board's `nginx -t`.

HTTP timing used six parallel connections and 72 requests per endpoint per
configuration. Six rounds alternated which configuration ran first. Responses
were fully read, and clients requested gzip. WebSocket checks overlapped the
HTTP/upload benchmark, so their latency samples do not represent identical
loads and must not be interpreted as a speedup comparison.

Memory was sampled after the workloads using `/proc/PID/smaps_rollup`, summing
PSS and private clean/dirty pages for each instance's master and workers. PSS
accounts proportionally for shared pages, including pages shared with the
production nginx instance. These are one-run process-footprint measurements,
not whole-system RAM savings or peak-buffer measurements.

## Final configuration results

| Measurement | Baseline | Proposed |
| --- | ---: | ---: |
| Workers | 4 as root | 2 as www-data |
| Master + workers PSS | 2,917 KiB | 1,983 KiB |
| Master + workers private memory | 1,608 KiB | 888 KiB |
| app.js median / p95, six concurrent clients | 11.86 / 18.00 ms | 11.69 / 18.49 ms |
| Instrument API median / p95, six concurrent clients | 421.21 / 430.90 ms | 420.44 / 433.99 ms |
| Valid FFT WebSocket replies | 600 / 600 | 600 / 600 |

The measured nginx PSS fell by 32.0%, and private memory by 44.8%. HTTP latency
was comparable; these measurements do not establish a throughput improvement.
The eight 8 KiB API body buffers reduce the configured buffer allowance from
512 to 64 KiB per response; the small API replies tested here do not exercise
that maximum.

Both configurations passed:

- HTTP 200 for instrument HTML/JS/CSS, management HTML/JS, instrument-list and
  details APIs, and the server's rate JSON. Static-file decoded hashes matched.
- Uploading a ZIP containing a version file and 1 MiB probe payload, then
  deleting it without running it. Both operations returned HTTP 200.
- HTTP 413 for an upload declaring a 21 MiB body, preserving the 20 MiB limit.
- Real FFT RPCs over the nginx WebSocket proxy: 600 replies, 1,024 float bins
  each, correct response headers/lengths, and 614,400 finite samples per
  configuration. Spectra changed during acquisition; some replies were cached.
- Unchanged FFT control readback before and after each WebSocket run.

The running FFT server retained PID 147. Production nginx configuration hashes
were unchanged, nginx/uWSGI/the instrument remained active, and the probe
archive was removed. The temporary nginx instances were stopped after testing.

## Compression regression caught on hardware

The first proposal (`7594d556`) added dynamic gzip for JS/CSS/JSON/SVG.
For the 180,288-byte `app.js`, this reduced transfer size to 51,315 bytes,
but median latency with six concurrent clients increased from 12.25 to
72.85 ms (p95: 17.03 to 81.60 ms). The final configuration removes those added
compression types and retains nginx's default HTML-only compression.

Precompressing static assets during packaging could retain the transfer-size
benefit without per-request compression, but was not implemented or tested here.

## Build and coverage limits

The 6 boot-extraction and 15 instrument-installation host regressions passed,
as did `git diff --check`. No OS image build, reboot/systemd-startup validation,
browser rendering test, or instrument replacement was performed on the board.
The tests validate the proposed nginx configuration against the existing board
runtime; they do not validate a newly built SD image or persistent deployment.
